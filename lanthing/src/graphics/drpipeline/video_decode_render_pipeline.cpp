#include "video_decode_render_pipeline.h"

#include <atomic>
#include <condition_variable>
#include <fstream>
#include <mutex>

#include <g3log/g3log.hpp>
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <SDL_syswm.h>

#include <ltlib/threads.h>
#include <ltlib/times.h>

#include "ct_smoother.h"
#include "gpu_capability.h"
#include <graphics/decoder/video_decoder.h>
#include <graphics/renderer/video_renderer.h>

namespace lt {

using namespace std::chrono_literals;

class VDRPipeline {
public:
    struct VideoFrameInternal : lt::VideoFrame {
        std::shared_ptr<uint8_t> data_internal;
    };

public:
    VDRPipeline(const VideoDecodeRenderPipeline::Params& params);
    ~VDRPipeline();
    bool init();
    void reset();
    VideoDecodeRenderPipeline::Action submit(const lt::VideoFrame& frame);

private:
    void decodeLoop(const std::function<void()>& i_am_alive);
    void renderLoop(const std::function<void()>& i_am_alive);

    bool waitForDecode(std::vector<VideoFrameInternal>& frames,
                       std::chrono::microseconds max_delay);
    bool waitForRender(std::chrono::microseconds ms);

private:
    const uint32_t width_;
    const uint32_t height_;
    const uint32_t screen_refresh_rate_;
    const lt::VideoCodecType codec_type_;
    std::function<void(uint32_t, std::shared_ptr<google::protobuf::MessageLite>, bool)>
        send_message_to_host_;
    HWND hwnd_;

    std::atomic<bool> request_i_frame_ = false;
    std::vector<VideoFrameInternal> encoded_frames_;

    bool decode_signal_ = false;
    std::mutex decode_mtx_;
    std::condition_variable waiting_for_decode_;

    bool render_signal_ = false;
    std::mutex render_mtx_;
    std::condition_variable waiting_for_render_;

    GpuInfo gpu_info_;
    std::unique_ptr<VideoRenderer> video_renderer_;
    std::unique_ptr<VideoDecoder> video_decoder_;
    CTSmoother smoother_;
    std::atomic<bool> stoped_{true};
    std::unique_ptr<ltlib::BlockingThread> decode_thread_;
    std::unique_ptr<ltlib::BlockingThread> render_thread_;
};

VDRPipeline::VDRPipeline(const VideoDecodeRenderPipeline::Params& params)
    : width_{params.width}
    , height_{params.height}
    , screen_refresh_rate_{params.screen_refresh_rate}
    , codec_type_{params.codec_type}
    , send_message_to_host_{params.send_message_to_host} {
    SDL_SysWMinfo info{};
    SDL_VERSION(&info.version);
    SDL_GetWindowWMInfo(params.sdl->window(), &info);
    hwnd_ = info.info.win.window;
}

VDRPipeline::~VDRPipeline() {
    stoped_ = true;
    decode_thread_.reset();
    render_thread_.reset();
    video_decoder_.reset();
    video_renderer_.reset();
}

bool VDRPipeline::init() {
    if (!gpu_info_.init()) {
        return false;
    }
    std::map<uint32_t, GpuInfo::Ability> sorted_by_memory;
    for (auto& ability : gpu_info_.get()) {
        sorted_by_memory.emplace(ability.video_memory_mb, ability);
    }
    if (sorted_by_memory.empty()) {
        LOG(WARNING) << "No hardware video decode ability!";
        return false;
    }
    uint64_t target_adapter = sorted_by_memory.rbegin()->second.luid;
    VideoRenderer::Params render_params{};
    render_params.window = hwnd_;
    render_params.device = target_adapter;
    render_params.video_width = width_;
    render_params.video_height = height_;
    // FIXME: align由解码器提供
    render_params.align = codec_type_ == lt::VideoCodecType::H264 ? 16 : 128;
    video_renderer_ = VideoRenderer::create(render_params);
    if (video_renderer_ == nullptr) {
        return false;
    }
    VideoDecoder::Params decode_params{};
    decode_params.codec_type = codec_type_;
    decode_params.hw_device = video_renderer_->hwDevice();
    decode_params.hw_context = video_renderer_->hwContext();
    decode_params.va_type = VaType::D3D11;
    decode_params.width = width_;
    decode_params.height = height_;
    video_decoder_ = VideoDecoder::create(decode_params);
    if (video_decoder_ == nullptr) {
        return false;
    }
    if (!video_renderer_->bindTextures(video_decoder_->textures())) {
        return false;
    }
    smoother_.clear();
    stoped_ = false;
    decode_thread_ = ltlib::BlockingThread::create(
        "video_decode",
        [this](const std::function<void()>& i_am_alive, void*) { decodeLoop(i_am_alive); },
        nullptr);
    render_thread_ = ltlib::BlockingThread::create(
        "video_render",
        [this](const std::function<void()>& i_am_alive, void*) { renderLoop(i_am_alive); },
        nullptr);
    return true;
}

void VDRPipeline::reset() {
    LOG(FATAL) << "VDRPipeline::reset() not implemented";
}

VideoDecodeRenderPipeline::Action VDRPipeline::submit(const lt::VideoFrame& _frame) {
    // static std::fstream stream{"./vidoe_stream",
    //                            std::ios::out | std::ios::binary | std::ios::trunc};
    // stream.write(reinterpret_cast<const char*>(_frame.data), _frame.size);
    // stream.flush();
    VideoFrameInternal frame{};
    frame.is_keyframe = _frame.is_keyframe;
    frame.ltframe_id = _frame.ltframe_id;
    frame.size = _frame.size;
    frame.width = _frame.width;
    frame.height = _frame.height;
    frame.capture_timestamp_us = _frame.capture_timestamp_us;
    frame.start_encode_timestamp_us = _frame.start_encode_timestamp_us;
    frame.end_encode_timestamp_us = _frame.end_encode_timestamp_us;
    frame.temporal_id = _frame.temporal_id;
    frame.data_internal = std::shared_ptr<uint8_t>(new uint8_t[_frame.size]);
    memcpy(frame.data_internal.get(), _frame.data, _frame.size);
    frame.data = frame.data_internal.get();
    {
        std::unique_lock<std::mutex> lock(decode_mtx_);
        encoded_frames_.emplace_back(frame);
        decode_signal_ = true;
    }
    waiting_for_decode_.notify_one();
    bool request_i_frame = request_i_frame_.exchange(false);
    return request_i_frame ? VideoDecodeRenderPipeline::Action::REQUEST_KEY_FRAME
                           : VideoDecodeRenderPipeline::Action::NONE;
}

bool VDRPipeline::waitForDecode(std::vector<VideoFrameInternal>& frames,
                                std::chrono::microseconds max_delay) {
    std::unique_lock<std::mutex> lock(decode_mtx_);
    if (encoded_frames_.empty()) {
        waiting_for_decode_.wait_for(lock, max_delay, [this]() { return decode_signal_; });
        decode_signal_ = false;
    }
    frames.swap(encoded_frames_);
    return !frames.empty();
}

void VDRPipeline::decodeLoop(const std::function<void()>& i_am_alive) {
    while (!stoped_) {
        i_am_alive();
        std::vector<VideoFrameInternal> frames;
        waitForDecode(frames, 5ms);
        if (frames.empty()) {
            continue;
        }
        for (auto& frame : frames) {
            DecodedFrame decoded_frame = video_decoder_->decode(frame.data, frame.size);
            if (decoded_frame.status == DecodeStatus::Failed) {
                LOG(WARNING) << "Failed to call decode(), reqesut i frame";
                request_i_frame_ = true;
                break;
            }
            else if (decoded_frame.status == DecodeStatus::EAgain) {
                LOG(FATAL) << "Should not be reach here";
            }
            else {
                CTSmoother::Frame f;
                f.no = decoded_frame.frame;
                f.capture_time = frame.capture_timestamp_us;
                f.at_time = ltlib::steady_now_us();
                {
                    std::unique_lock<std::mutex> lock(render_mtx_);
                    smoother_.push(f);
                }
                waiting_for_render_.notify_one();
            }
        }
    }
}

bool VDRPipeline::waitForRender(std::chrono::microseconds ms) {
    std::unique_lock<std::mutex> lock(render_mtx_);
    bool ret = waiting_for_render_.wait_for(lock, ms, [this]() { return smoother_.size() > 0; });
    return ret;
}

void VDRPipeline::renderLoop(const std::function<void()>& i_am_alive) {
    while (!stoped_) {
        i_am_alive();
        ltlib::Timestamp cur_time = ltlib::Timestamp::now();
        if (video_renderer_->waitForPipeline(16) && waitForRender(2ms)) {
            int64_t frame = smoother_.get(cur_time.microseconds());
            smoother_.pop();
            if (frame != -1) {
                video_renderer_->render(frame);
            }
        }
    }
}

VideoDecodeRenderPipeline::Params::Params(
    lt::VideoCodecType _codec_type, uint32_t _width, uint32_t _height,
    uint32_t _screen_refresh_rate,
    std::function<void(uint32_t, std::shared_ptr<google::protobuf::MessageLite>, bool)>
        send_message)
    : codec_type(_codec_type)
    , width(_width)
    , height(_height)
    , screen_refresh_rate(_screen_refresh_rate)
    , send_message_to_host(send_message) {}

bool VideoDecodeRenderPipeline::Params::validate() const {
    if (codec_type == lt::VideoCodecType::Unknown || sdl == nullptr ||
        send_message_to_host == nullptr) {
        return false;
    }
    else {
        return true;
    }
}

std::unique_ptr<VideoDecodeRenderPipeline> VideoDecodeRenderPipeline::create(const Params& params) {
    if (!params.validate()) {
        LOG(FATAL) << "Create VideoDecodeRenderPipeline failed: invalid parameter";
        return nullptr;
    }
    auto impl = std::make_shared<VDRPipeline>(params);
    if (!impl->init()) {
        return nullptr;
    }
    std::unique_ptr<VideoDecodeRenderPipeline> decoder{new VideoDecodeRenderPipeline};
    decoder->impl_ = std::move(impl);
    return decoder;
}

void VideoDecodeRenderPipeline::reset() {
    impl_->reset();
}

VideoDecodeRenderPipeline::Action VideoDecodeRenderPipeline::submit(const lt::VideoFrame& frame) {
    return impl_->submit(frame);
}

} // namespace lt