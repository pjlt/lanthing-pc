/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2023 Zhennan Tu <zhennan.tu@gmail.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "video_decode_render_pipeline.h"

#include <atomic>
#include <condition_variable>
#include <fstream>
#include <mutex>
#include <tuple>

#include <ltlib/logging.h>
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <SDL_syswm.h>

#include <ltproto/ltproto.h>
#include <ltproto/worker2service/reconfigure_video_encoder.pb.h>

#include <ltlib/threads.h>
#include <ltlib/times.h>

#include "ct_smoother.h"
#include "gpu_capability.h"
#include <graphics/decoder/video_decoder.h>
#include <graphics/drpipeline/video_statistics.h>
#include <graphics/renderer/video_renderer.h>
#include <graphics/widgets/widgets_manager.h>

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
    VideoDecodeRenderPipeline::Action submit(const lt::VideoFrame& frame);
    void setTimeDiff(int64_t diff_us);
    void setRTT(int64_t rtt_us);
    void setBWE(uint32_t bps);
    void setNack(uint32_t nack);
    void setLossRate(float rate);
    void resetRenderTarget();
    void setCursorInfo(int32_t cursor_id, float x, float y, bool visible);
    void switchMouseMode(bool absolute);

private:
    void decodeLoop(const std::function<void()>& i_am_alive);
    void renderLoop(const std::function<void()>& i_am_alive);

    bool waitForDecode(std::vector<VideoFrameInternal>& frames,
                       std::chrono::microseconds max_delay);
    bool waitForRender(std::chrono::microseconds ms);
    void onStat();
    void onUserSetBitrate(uint32_t bps);
    std::tuple<int32_t, float, float> getCursorInfo();
    bool isAbsoluteMouse();

private:
    const uint32_t width_;
    const uint32_t height_;
    const uint32_t screen_refresh_rate_;
    const lt::VideoCodecType codec_type_;
    std::function<void(uint32_t, std::shared_ptr<google::protobuf::MessageLite>, bool)>
        send_message_to_host_;
    PcSdl* sdl_;
    void* window_;

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

    bool show_statistics_ = true;
    bool show_status_ = true;
    std::unique_ptr<WidgetsManager> widgets_;
    std::unique_ptr<VideoStatistics> statistics_;
    std::unique_ptr<ltlib::TaskThread> stat_thread_;
    int64_t time_diff_ = 0;
    int64_t rtt_ = 0;
    uint32_t bwe_ = 0;
    uint32_t nack_ = 0;
    float loss_rate_ = .0f;

    int32_t cursor_id_ = 0;
    float cursor_x_ = 0.f;
    float cursor_y_ = 0.f;
    bool visible_ = true;
    bool absolute_mouse_ = true;
};

VDRPipeline::VDRPipeline(const VideoDecodeRenderPipeline::Params& params)
    : width_{params.width}
    , height_{params.height}
    , screen_refresh_rate_{params.screen_refresh_rate}
    , codec_type_{params.codec_type}
    , send_message_to_host_{params.send_message_to_host}
    , sdl_{params.sdl}
    , statistics_{new VideoStatistics} {
    window_ = params.sdl->window();
}

VDRPipeline::~VDRPipeline() {
    stoped_ = true;
    decode_thread_.reset();
    render_thread_.reset();
    video_decoder_.reset();
    video_renderer_.reset();
}

bool VDRPipeline::init() {
    VideoRenderer::Params render_params{};
#if LT_WINDOWS
    if (!gpu_info_.init()) {
        return false;
    }
    std::map<uint32_t, GpuInfo::Ability> sorted_by_memory;
    for (auto& ability : gpu_info_.get()) {
        sorted_by_memory.emplace(ability.video_memory_mb, ability);
    }
    if (sorted_by_memory.empty()) {
        LOG(ERR) << "No hardware video decode ability!";
        return false;
    }
    uint64_t target_adapter = sorted_by_memory.rbegin()->second.luid;
    render_params.device = target_adapter;
#elif LT_LINUX
    render_params.device = 0;
#else
#endif
    render_params.window = window_;
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
#if LT_WINDOWS
    decode_params.va_type = VaType::D3D11;
#elif LT_LINUX
    decode_params.va_type = VaType::VAAPI;
#else
#error unknown platform
#endif
    decode_params.width = width_;
    decode_params.height = height_;
    video_decoder_ = VideoDecoder::create(decode_params);
    if (video_decoder_ == nullptr) {
        return false;
    }
    if (!video_renderer_->bindTextures(video_decoder_->textures())) {
        return false;
    }
    WidgetsManager::Params widgets_params{};
    widgets_params.dev = video_renderer_->hwDevice();
    widgets_params.ctx = video_renderer_->hwContext();
    widgets_params.window = window_;
    widgets_params.video_width = width_;
    widgets_params.video_height = height_;
    widgets_params.set_bitrate =
        std::bind(&VDRPipeline::onUserSetBitrate, this, std::placeholders::_1);
    widgets_ = WidgetsManager::create(widgets_params);
    if (widgets_ == nullptr) {
        return false;
    }
    smoother_.clear();
    stoped_ = false;
    decode_thread_ = ltlib::BlockingThread::create(
        "video_decode",
        [this](const std::function<void()>& i_am_alive) { decodeLoop(i_am_alive); });
    render_thread_ = ltlib::BlockingThread::create(
        "video_render",
        [this](const std::function<void()>& i_am_alive) { renderLoop(i_am_alive); });
    stat_thread_ = ltlib::TaskThread::create("stat_task");
    stat_thread_->post_delay(ltlib::TimeDelta{1'000'00}, std::bind(&VDRPipeline::onStat, this));
    return true;
}

VideoDecodeRenderPipeline::Action VDRPipeline::submit(const lt::VideoFrame& _frame) {
    // static std::fstream stream{"./vidoe_stream",
    //                            std::ios::out | std::ios::binary | std::ios::trunc};
    // stream.write(reinterpret_cast<const char*>(_frame.data), _frame.size);
    // stream.flush();
    LOGF(DEBUG, "capture:%" PRId64 ", start_enc:% " PRId64 ", end_enc:%" PRId64,
         _frame.capture_timestamp_us, _frame.start_encode_timestamp_us,
         _frame.end_encode_timestamp_us);
    statistics_->addEncode();
    statistics_->updateVideoBW(_frame.size);
    statistics_->updateEncodeTime(_frame.end_encode_timestamp_us -
                                  _frame.start_encode_timestamp_us);
    if (time_diff_ != 0) {
        statistics_->updateNetDelay(ltlib::steady_now_us() - _frame.end_encode_timestamp_us -
                                    time_diff_);
    }

    VideoFrameInternal frame{};
    frame.is_keyframe = _frame.is_keyframe;
    frame.ltframe_id = _frame.ltframe_id;
    frame.size = _frame.size;
    frame.width = _frame.width;
    frame.height = _frame.height;
    frame.capture_timestamp_us = _frame.capture_timestamp_us;
    frame.start_encode_timestamp_us = _frame.start_encode_timestamp_us;
    frame.end_encode_timestamp_us = _frame.end_encode_timestamp_us;
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

void VDRPipeline::setTimeDiff(int64_t diff_us) {
    LOG(DEBUG) << "TIME DIFF " << diff_us;
    time_diff_ = diff_us;
}

void VDRPipeline::setRTT(int64_t rtt_us) {
    rtt_ = rtt_us;
}

void VDRPipeline::setBWE(uint32_t bps) {
    bwe_ = bps;
    statistics_->updateBWE(bps);
}

void VDRPipeline::setNack(uint32_t nack) {
    nack_ = nack;
}

void VDRPipeline::setLossRate(float rate) {
    loss_rate_ = rate;
}

void VDRPipeline::resetRenderTarget() {
    video_renderer_->resetRenderTarget();
}

void VDRPipeline::setCursorInfo(int32_t cursor_id, float x, float y, bool visible) {
    std::lock_guard lk{render_mtx_};
    cursor_id_ = cursor_id;
    cursor_x_ = x;
    cursor_y_ = y;
    visible_ = visible;
}

void VDRPipeline::switchMouseMode(bool absolute) {
    std::lock_guard lk{render_mtx_};
    absolute_mouse_ = absolute;
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
            auto start = ltlib::steady_now_us();
            DecodedFrame decoded_frame = video_decoder_->decode(frame.data, frame.size);
            auto end = ltlib::steady_now_us();
            if (decoded_frame.status == DecodeStatus::Failed) {
                LOG(ERR) << "Failed to call decode(), reqesut i frame";
                request_i_frame_ = true;
                break;
            }
            else if (decoded_frame.status == DecodeStatus::EAgain) {
                LOG(FATAL) << "Should not be reach here";
            }
            else {
                LOG(DEBUG) << "CAPTURE-AFTER_DECODE "
                           << ltlib::steady_now_us() - frame.capture_timestamp_us - time_diff_;
                statistics_->updateDecodeTime(end - start);
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

void VDRPipeline::onStat() {
    auto stat = statistics_->getStat();
    if (show_statistics_) {
        widgets_->updateStatistics(stat);
    }
    if (show_statistics_) {
        widgets_->updateStatus((uint32_t)rtt_ / 1000, (uint32_t)stat.render_video_fps, loss_rate_);
    }
    stat_thread_->post_delay(ltlib::TimeDelta{1'000'00}, std::bind(&VDRPipeline::onStat, this));
}

void VDRPipeline::onUserSetBitrate(uint32_t bps) {
    auto msg = std::make_shared<ltproto::worker2service::ReconfigureVideoEncoder>();
    if (bps == 0) {
        msg->set_trigger(ltproto::worker2service::ReconfigureVideoEncoder_Trigger_TurnOnAuto);
    }
    else {
        msg->set_trigger(ltproto::worker2service::ReconfigureVideoEncoder_Trigger_TurnOffAuto);
        msg->set_bitrate_bps(bps);
    }
    send_message_to_host_(ltproto::id(msg), msg, true);
}

std::tuple<int32_t, float, float> VDRPipeline::getCursorInfo() {
    std::lock_guard lk{render_mtx_};
    return {cursor_id_, cursor_x_, cursor_y_};
}

bool VDRPipeline::isAbsoluteMouse() {
    std::lock_guard lk{render_mtx_};
    return absolute_mouse_;
}

void VDRPipeline::renderLoop(const std::function<void()>& i_am_alive) {
    while (!stoped_) {
        i_am_alive();
        ltlib::Timestamp cur_time = ltlib::Timestamp::now();
        if (video_renderer_->waitForPipeline(16) && waitForRender(2ms)) {
            auto frame = smoother_.get(cur_time.microseconds());
            smoother_.pop();
            video_renderer_->switchMouseMode(isAbsoluteMouse());
            if (frame.has_value()) {
                auto [cursor, x, y] = getCursorInfo();
                video_renderer_->updateCursor(cursor, x, y, visible_);
                LOG(DEBUG) << "CAPTURE-BEFORE_RENDER "
                           << ltlib::steady_now_us() - frame->capture_time - time_diff_;
                statistics_->addRenderVideo();
                auto start = ltlib::steady_now_us();
                auto result = video_renderer_->render(frame->no);
                auto end = ltlib::steady_now_us();
                switch (result) {
                case VideoRenderer::RenderResult::Failed:
                    // TODO: 更好地通知退出
                    LOG(ERR) << "Render failed, exit render loop";
                    return;
                case VideoRenderer::RenderResult::Reset:
                    widgets_->reset();
                    break;
                case VideoRenderer::RenderResult::Success2:
                default:
                    break;
                }
                statistics_->updateRenderVideoTime(end - start);
            }
            auto start = ltlib::steady_now_us();
            widgets_->render();
            auto mid = ltlib::steady_now_us();
            video_renderer_->present();
            auto end = ltlib::steady_now_us();
            statistics_->addPresent();
            statistics_->updateRenderWidgetsTime(mid - start);
            statistics_->updatePresentTime(end - mid);
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
    std::unique_ptr<VideoDecodeRenderPipeline> pipeline{new VideoDecodeRenderPipeline};
    pipeline->impl_ = std::move(impl);
    return pipeline;
}

VideoDecodeRenderPipeline::Action VideoDecodeRenderPipeline::submit(const lt::VideoFrame& frame) {
    return impl_->submit(frame);
}

void VideoDecodeRenderPipeline::resetRenderTarget() {
    impl_->resetRenderTarget();
}

void VideoDecodeRenderPipeline::setTimeDiff(int64_t diff_us) {
    impl_->setTimeDiff(diff_us);
}

void VideoDecodeRenderPipeline::setRTT(int64_t rtt_us) {
    impl_->setRTT(rtt_us);
}

void VideoDecodeRenderPipeline::setBWE(uint32_t bps) {
    impl_->setBWE(bps);
}

void VideoDecodeRenderPipeline::setNack(uint32_t nack) {
    impl_->setNack(nack);
}

void VideoDecodeRenderPipeline::setLossRate(float rate) {
    impl_->setLossRate(rate);
}

void VideoDecodeRenderPipeline::setCursorInfo(int32_t cursor_id, float x, float y, bool visible) {
    impl_->setCursorInfo(cursor_id, x, y, visible);
}

void VideoDecodeRenderPipeline::switchMouseMode(bool absolute) {
    impl_->switchMouseMode(absolute);
}

} // namespace lt