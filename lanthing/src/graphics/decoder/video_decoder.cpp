// ffmpeg头文件有警告
#pragma warning(disable : 4244)
#include "video_decoder.h"

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

#include <transport/transport.h>

#include "ct_smoother.h"
#include "d3d11_pipeline.h"
#include "gpu_capability.h"

namespace lt {

using namespace std::chrono_literals;

class VideoDecoderImpl {
public:
    struct VideoFrameInternal : lt::VideoFrame {
        std::shared_ptr<uint8_t> data_internal;
    };

public:
    VideoDecoderImpl(const VideoDecoder::Params& params);
    ~VideoDecoderImpl();
    bool init();
    void reset_deocder_renderer();
    VideoDecoder::Action submit(const lt::VideoFrame& frame);

private:
    void decode_loop(const std::function<void()>& i_am_alive);
    void render_loop(const std::function<void()>& i_am_alive);

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
    std::unique_ptr<D3D11Pipeline> d3d11_pipe_line_;
    CTSmoother smoother_;
    std::atomic<bool> stoped_{true};
    std::unique_ptr<ltlib::BlockingThread> decode_thread_;
    std::unique_ptr<ltlib::BlockingThread> render_thread_;
};

VideoDecoderImpl::VideoDecoderImpl(const VideoDecoder::Params& params)
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

VideoDecoderImpl::~VideoDecoderImpl() {
    stoped_ = true;
    decode_thread_.reset();
    render_thread_.reset();
    d3d11_pipe_line_.reset();
}

bool VideoDecoderImpl::init() {
    uint64_t target_adapter = 0;
    Format target_format;
    switch (codec_type_) {
    case lt::VideoCodecType::H264:
        target_format = Format::H264_NV12;
        break;
    case lt::VideoCodecType::H265:
        target_format = Format::H265_NV12;
        break;
    default:
        return false;
    }
    if (gpu_info_.init()) {
        std::map<uint32_t, GpuInfo::Ability> sorted_by_memory;
        for (auto& ability : gpu_info_.get()) {
            sorted_by_memory.emplace(ability.video_memory_mb, ability);
        }
        if (!sorted_by_memory.empty()) {
            target_adapter = sorted_by_memory.rbegin()->second.luid;
            LOGF(INFO, "try to setup d3d11 pipeline on %s",
                 sorted_by_memory.rbegin()->second.to_str().c_str());
        }
    }
    d3d11_pipe_line_ = std::make_unique<D3D11Pipeline>();
    if (!d3d11_pipe_line_->init(target_adapter)) {
        LOG(WARNING) << "Failed to initialize d3d11 pipeline on apdater " << 0;
        d3d11_pipe_line_ = nullptr;
        return false;
    }
    if (!d3d11_pipe_line_->setupRender((HWND)hwnd_, width_, height_)) {
        LOGF(WARNING, "Failed to setup d3d11 pipeline[render] on 0x%08x", hwnd_);
        d3d11_pipe_line_ = nullptr;
        return false;
    }
    if (!d3d11_pipe_line_->setupDecoder(target_format)) {
        LOG(WARNING) << "Failed to setup d3d11 pipeline[decoder]";
        return false;
    }
    smoother_.clear();
    stoped_ = false;
    decode_thread_ = ltlib::BlockingThread::create(
        "decode",
        [this](const std::function<void()>& i_am_alive, void*) { decode_loop(i_am_alive); },
        nullptr);
    render_thread_ = ltlib::BlockingThread::create(
        "render",
        [this](const std::function<void()>& i_am_alive, void*) { render_loop(i_am_alive); },
        nullptr);
    return true;
}

void VideoDecoderImpl::reset_deocder_renderer() {
    LOG(FATAL) << "reset_deocder_renderer() not implemented";
}

VideoDecoder::Action VideoDecoderImpl::submit(const lt::VideoFrame& _frame) {
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
        waiting_for_decode_.notify_one();
    }
    bool request_i_frame = request_i_frame_.exchange(false);
    return request_i_frame ? VideoDecoder::Action::REQUEST_KEY_FRAME : VideoDecoder::Action::NONE;
}

bool VideoDecoderImpl::waitForDecode(std::vector<VideoFrameInternal>& frames,
                                     std::chrono::microseconds max_delay) {
    std::unique_lock<std::mutex> lock(decode_mtx_);
    if (encoded_frames_.empty()) {
        waiting_for_decode_.wait_for(lock, max_delay, [this]() { return decode_signal_; });
        decode_signal_ = false;
    }
    frames.swap(encoded_frames_);
    return !frames.empty();
}

void VideoDecoderImpl::decode_loop(const std::function<void()>& i_am_alive) {
    while (!stoped_) {
        i_am_alive();
        std::vector<VideoFrameInternal> frames;
        waitForDecode(frames, 5ms);
        if (frames.empty()) {
            continue;
        }
        for (auto& frame : frames) {
            auto resouce_id = d3d11_pipe_line_->decode(frame.data, frame.size);
            if (resouce_id < 0) {
                LOG(WARNING) << "failed to call decode(), reqesut i frame";
                request_i_frame_ = true;
                break;
            }
            CTSmoother::Frame f;
            f.no = resouce_id;
            f.capture_time = frame.capture_timestamp_us;
            f.at_time = ltlib::steady_now_us();
            smoother_.push(f);
        }
        std::unique_lock<std::mutex> lock(render_mtx_);
        waiting_for_render_.notify_one();
    }
}

bool VideoDecoderImpl::waitForRender(std::chrono::microseconds ms) {
    std::unique_lock<std::mutex> lock(render_mtx_);
    bool ret = waiting_for_render_.wait_for(lock, ms, [this]() { return smoother_.size() > 0; });
    return ret;
}

void VideoDecoderImpl::render_loop(const std::function<void()>& i_am_alive) {
    while (!stoped_) {
        i_am_alive();
        ltlib::Timestamp cur_time = ltlib::Timestamp::now();
        if (d3d11_pipe_line_->waitForPipeline(16) && waitForRender(2ms)) {
            auto frame = smoother_.get(cur_time.microseconds());
            smoother_.pop();
            if (frame > 0) {
                d3d11_pipe_line_->render(frame);
            }
        }
    }
}

VideoDecoder::Params::Params(
    lt::VideoCodecType _codec_type, uint32_t _width, uint32_t _height,
    uint32_t _screen_refresh_rate,
    std::function<void(uint32_t, std::shared_ptr<google::protobuf::MessageLite>, bool)>
        send_message)
    : codec_type(_codec_type)
    , width(_width)
    , height(_height)
    , screen_refresh_rate(_screen_refresh_rate)
    , send_message_to_host(send_message) {}

bool VideoDecoder::Params::validate() const {
    if (codec_type == lt::VideoCodecType::Unknown || sdl == nullptr ||
        send_message_to_host == nullptr) {
        return false;
    }
    else {
        return true;
    }
}

std::unique_ptr<VideoDecoder> VideoDecoder::create(const Params& params) {
    if (!params.validate()) {
        LOG(FATAL) << "Create video module failed: invalid parameter";
        return nullptr;
    }
    auto impl = std::make_shared<VideoDecoderImpl>(params);
    if (!impl->init()) {
        return nullptr;
    }
    std::unique_ptr<VideoDecoder> decoder{new VideoDecoder};
    decoder->impl_ = std::move(impl);
    return decoder;
}

void VideoDecoder::reset_decoder_renderer() {
    impl_->reset_deocder_renderer();
}

VideoDecoder::Action VideoDecoder::submit(const lt::VideoFrame& frame) {
    return impl_->submit(frame);
}

} // namespace lt