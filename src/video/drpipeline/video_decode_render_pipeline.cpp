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

#include <ltproto/client2worker/switch_monitor.pb.h>
#include <ltproto/ltproto.h>
#include <ltproto/worker2service/reconfigure_video_encoder.pb.h>

#include <ltlib/threads.h>
#include <ltlib/times.h>

#include <video/decoder/video_decoder.h>
#include <video/drpipeline/ct_smoother.h>
#include <video/drpipeline/video_statistics.h>
#include <video/renderer/video_renderer.h>
#include <video/widgets/widgets_manager.h>

namespace lt {

namespace video {

using namespace std::chrono_literals;

class VDRPipeline2 : public DecodeRenderPipeline {
public:
    static std::unique_ptr<VDRPipeline2> create(const DecodeRenderPipeline::Params& params);
    ~VDRPipeline2() override;
    DecodeRenderPipeline::Action submit(const lt::VideoFrame& frame) override;
    void setTimeDiff(int64_t diff_us) override;
    void setRTT(int64_t rtt_us) override;
    void setBWE(uint32_t bps) override;
    void setNack(uint32_t nack) override;
    void setLossRate(float rate) override;
    void resetRenderTarget() override;
    void setCursorInfo(const ::lt::CursorInfo& info) override;
    void switchMouseMode(bool absolute) override;
    void switchStretchMode(bool stretch) override;

protected:
    VDRPipeline2();
};

VDRPipeline2::VDRPipeline2() = default;
VDRPipeline2::~VDRPipeline2() = default;
DecodeRenderPipeline::Action VDRPipeline2::submit(const lt::VideoFrame&) {
    return DecodeRenderPipeline::Action::NONE;
}
void VDRPipeline2::setTimeDiff(int64_t) {}
void VDRPipeline2::setRTT(int64_t) {}
void VDRPipeline2::setBWE(uint32_t) {}
void VDRPipeline2::setNack(uint32_t) {}
void VDRPipeline2::setLossRate(float) {}
void VDRPipeline2::resetRenderTarget() {}
void VDRPipeline2::setCursorInfo(const ::lt::CursorInfo&) {}
void VDRPipeline2::switchMouseMode(bool) {}
void VDRPipeline2::switchStretchMode(bool) {}
// #if !defined(LT_WINDOWS)
std::unique_ptr<VDRPipeline2> VDRPipeline2::create(const DecodeRenderPipeline::Params&) {
    return nullptr;
}
// #endif // LT_WINDOWS

class VDRPipeline : public DecodeRenderPipeline {
public:
    struct VideoFrameInternal : lt::VideoFrame {
        std::shared_ptr<uint8_t[]> data_internal;
    };

public:
    static std::unique_ptr<VDRPipeline> create(const DecodeRenderPipeline::Params& params);
    ~VDRPipeline() override;
    DecodeRenderPipeline::Action submit(const lt::VideoFrame& frame) override;
    void setTimeDiff(int64_t diff_us) override;
    void setRTT(int64_t rtt_us) override;
    void setBWE(uint32_t bps) override;
    void setNack(uint32_t nack) override;
    void setLossRate(float rate) override;
    void resetRenderTarget() override;
    void setCursorInfo(const ::lt::CursorInfo& info) override;
    void switchMouseMode(bool absolute) override;
    void switchStretchMode(bool stretch) override;

private:
    VDRPipeline(const DecodeRenderPipeline::Params& params);
    bool init();
    void decodeLoop(const std::function<void()>& i_am_alive);
    void renderLoop(const std::function<void()>& i_am_alive);

    bool waitForDecode(std::vector<VideoFrameInternal>& frames,
                       std::chrono::microseconds max_delay);
    bool waitForRender(std::chrono::microseconds ms);
    void onStat();
    void onUserSetBitrate(uint32_t bps);
    void onUserSwitchMonitor();
    void onUserSwitchStretchOrOrigin();
    bool isAbsoluteMouse();
    bool isStretchMode();

private:
    const bool for_test_;
    const uint32_t width_;
    const uint32_t height_;
    const uint32_t screen_refresh_rate_;
    const uint32_t rotation_;
    const lt::VideoCodecType encode_codec_type_;
    const lt::VideoCodecType decode_codec_type_;
    std::function<void(uint32_t, std::shared_ptr<google::protobuf::MessageLite>, bool)>
        send_message_to_host_;
    std::function<void()> switch_stretch_;
    std::function<void()> reset_pipeline_;
    lt::plat::PcSdl* sdl_;
    void* window_;

    std::atomic<bool> request_i_frame_ = false;
    std::vector<VideoFrameInternal> encoded_frames_;

    bool decode_signal_ = false;
    std::mutex decode_mtx_;
    std::condition_variable waiting_for_decode_;

    bool render_signal_ = false;
    std::mutex render_mtx_;
    std::condition_variable waiting_for_render_;

    std::unique_ptr<Renderer> video_renderer_;
    std::unique_ptr<Decoder> video_decoder_;
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

    std::optional<lt::CursorInfo> cursor_info_;
    bool absolute_mouse_;
    bool is_stretch_;
    int64_t status_color_;
    void* device_;  // not ref
    void* context_; // not ref
};

VDRPipeline::VDRPipeline(const DecodeRenderPipeline::Params& params)
    : for_test_{params.for_test}
    , width_{params.width}
    , height_{params.height}
    , screen_refresh_rate_{params.screen_refresh_rate}
    , rotation_{params.rotation}
    , encode_codec_type_{params.encode_codec}
    , decode_codec_type_{params.decode_codec}
    , send_message_to_host_{params.send_message_to_host}
    , switch_stretch_{params.switch_stretch}
    , reset_pipeline_{params.reset_pipeline}
    , sdl_{params.sdl}
    , statistics_{new VideoStatistics}
    , absolute_mouse_{params.absolute_mouse}
    , is_stretch_{params.stretch}
    , status_color_{params.status_color}
    , device_{params.device}
    , context_{params.context} {
    window_ = params.sdl->window();
}

std::unique_ptr<VDRPipeline> VDRPipeline::create(const DecodeRenderPipeline::Params& params) {
    std::unique_ptr<VDRPipeline> pipeline{new VDRPipeline(params)};
    if (pipeline->init()) {
        return pipeline;
    }
    else {
        return nullptr;
    }
}

VDRPipeline::~VDRPipeline() {
    stoped_ = true;
    decode_thread_.reset();
    render_thread_.reset();
    video_decoder_.reset();
    video_renderer_.reset();
}

bool VDRPipeline::init() {
    LOGF(INFO, "VDRPipeline w:%u, h:%u, r:%u codec:%s", width_, height_, rotation_,
         toString(decode_codec_type_));
    Renderer::Params render_params{};
    uint32_t video_width = width_;
    uint32_t video_height = height_;
    if (rotation_ == 90 || rotation_ == 270) {
        video_width = height_;
        video_height = width_;
    }
    render_params.window = window_;
    render_params.device = device_;
    render_params.context = context_;
    render_params.video_width = video_width;
    render_params.video_height = video_height;
    render_params.rotation = rotation_;
    render_params.stretch = is_stretch_;
    render_params.absolute_mouse = absolute_mouse_;
    render_params.align = Decoder::align(decode_codec_type_);

    video_renderer_ = Renderer::create(render_params);
    if (video_renderer_ == nullptr) {
        LOG(ERR) << "create renderer failed";
        return false;
    }
    Decoder::Params decode_params{};
    decode_params.codec_type = decode_codec_type_;
    decode_params.hw_device = video_renderer_->hwDevice();
    decode_params.hw_context = video_renderer_->hwContext();
#if LT_WINDOWS
    decode_params.va_type = VaType::D3D11;
#elif LT_LINUX
    decode_params.va_type = VaType::VAAPI;
#elif LT_MAC
    decode_params.va_type = VaType::VTB;
#else
#error unknown platform
#endif
    decode_params.width = video_width;
    decode_params.height = video_height;

    video_decoder_ = Decoder::create(decode_params);
    if (video_decoder_ == nullptr) {
        LOG(ERR) << "create decoder failed";

        return false;
    }
    if (!video_renderer_->setDecodedFormat(video_decoder_->decodedFormat())) {
        LOG(ERR) << "setdecodedformat failed";

        return false;
    }
    if (for_test_) {
        return true;
    }
    if (!video_renderer_->bindTextures(video_decoder_->textures())) {
        LOG(ERR) << "bind texture failed";

        return false;
    }
    WidgetsManager::Params widgets_params{};
    widgets_params.dev = video_renderer_->hwDevice();
    widgets_params.ctx = video_renderer_->hwContext();
    widgets_params.window = window_;
    widgets_params.video_width = width_;
    widgets_params.video_height = height_;
    widgets_params.status_color = status_color_;
    widgets_params.set_bitrate =
        std::bind(&VDRPipeline::onUserSetBitrate, this, std::placeholders::_1);
    widgets_params.switch_monitor = std::bind(&VDRPipeline::onUserSwitchMonitor, this);
    widgets_params.stretch = std::bind(&VDRPipeline::onUserSwitchStretchOrOrigin, this);
    widgets_ = WidgetsManager::create(widgets_params);
    if (widgets_ == nullptr) {
        LOG(ERR) << "create widgets failed";

        return false;
    }
    smoother_.clear();
    stoped_ = false;
    decode_thread_ = ltlib::BlockingThread::create(
        "lt_video_decode",
        [this](const std::function<void()>& i_am_alive) { decodeLoop(i_am_alive); });
    render_thread_ = ltlib::BlockingThread::create(
        "lt_video_render",
        [this](const std::function<void()>& i_am_alive) { renderLoop(i_am_alive); });
    stat_thread_ = ltlib::TaskThread::create("lt_stat_task");
    stat_thread_->post_delay(ltlib::TimeDelta{1'000'00}, std::bind(&VDRPipeline::onStat, this));
    return true;
}

DecodeRenderPipeline::Action VDRPipeline::submit(const lt::VideoFrame& _frame) {
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
    frame.data_internal = std::shared_ptr<uint8_t[]>(new uint8_t[_frame.size]);
    memcpy(frame.data_internal.get(), _frame.data, _frame.size);
    frame.data = frame.data_internal.get();
    {
        std::unique_lock<std::mutex> lock(decode_mtx_);
        encoded_frames_.emplace_back(frame);
        decode_signal_ = true;
    }
    waiting_for_decode_.notify_one();
    bool request_i_frame = request_i_frame_.exchange(false);
    return request_i_frame ? DecodeRenderPipeline::Action::REQUEST_KEY_FRAME
                           : DecodeRenderPipeline::Action::NONE;
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

void VDRPipeline::setCursorInfo(const ::lt::CursorInfo& info) {
    {
        std::lock_guard lk{render_mtx_};
        cursor_info_ = info;
    }
    waiting_for_render_.notify_one();
}

void VDRPipeline::switchMouseMode(bool absolute) {
    std::lock_guard lk{render_mtx_};
    absolute_mouse_ = absolute;
}

void VDRPipeline::switchStretchMode(bool stretch) {
    std::lock_guard lk{render_mtx_};
    is_stretch_ = stretch;
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
                LOG(ERR) << "Decode return EAgain(should not be reach here), try reset pipeline";
                reset_pipeline_();
            }
            else if (decoded_frame.status == DecodeStatus::NeedReset) {
                LOG(ERR) << "Decode return NeedReset, reset pipeline";
                reset_pipeline_();
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
    bool ret = waiting_for_render_.wait_for(
        lock, ms, [this]() { return smoother_.size() > 0 || cursor_info_.has_value(); });
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

void VDRPipeline::onUserSwitchMonitor() {
    auto msg = std::make_shared<ltproto::client2worker::SwitchMonitor>();
    send_message_to_host_(ltproto::id(msg), msg, true);
}

void VDRPipeline::onUserSwitchStretchOrOrigin() {
    // 跑在渲染线程
    switch_stretch_();
}

bool VDRPipeline::isAbsoluteMouse() {
    std::lock_guard lk{render_mtx_};
    return absolute_mouse_;
}

bool VDRPipeline::isStretchMode() {
    std::lock_guard lk{render_mtx_};
    return is_stretch_;
}

void VDRPipeline::renderLoop(const std::function<void()>& i_am_alive) {
    std::optional<CTSmoother::Frame> frame;
    while (!stoped_) {
        i_am_alive();
        ltlib::Timestamp cur_time = ltlib::Timestamp::now();
        if (video_renderer_->waitForPipeline(16)) {
            waitForRender(16ms);
            auto new_frame = smoother_.get(cur_time.microseconds());
            smoother_.pop();
            if (new_frame.has_value()) {
                frame = new_frame;
            }
            if (!frame.has_value()) {
                continue;
            }
            video_renderer_->switchMouseMode(isAbsoluteMouse());
            video_renderer_->switchStretchMode(isStretchMode());
            std::optional<lt::CursorInfo> cursor_info;
            {
                std::lock_guard lk{render_mtx_};
                std::swap(cursor_info, cursor_info_);
            }
            video_renderer_->updateCursor(cursor_info);
            LOG(DEBUG) << "CAPTURE-BEFORE_RENDER "
                       << ltlib::steady_now_us() - frame->capture_time - time_diff_;
            if (new_frame.has_value()) {
                statistics_->addRenderVideo();
            }
            auto t0 = ltlib::steady_now_us();
            auto result = video_renderer_->render(frame->no);
            auto t1 = ltlib::steady_now_us();
            switch (result) {
            case Renderer::RenderResult::Failed:
                // TODO: 更好地通知退出
                LOG(ERR) << "Render failed, exit render loop";
                return;
            case Renderer::RenderResult::Reset:
                widgets_->reset();
                break;
            case Renderer::RenderResult::Success2:
            default:
                break;
            }
            statistics_->updateRenderVideoTime(t1 - t0);
            auto t2 = ltlib::steady_now_us();
            widgets_->render();
            auto t3 = ltlib::steady_now_us();
            video_renderer_->present();
            auto t4 = ltlib::steady_now_us();
            statistics_->addPresent();
            statistics_->updateRenderWidgetsTime(t3 - t2);
            statistics_->updatePresentTime(t4 - t3);
        }
    }
}

DecodeRenderPipeline::Params::Params(
    lt::VideoCodecType encode, lt::VideoCodecType decode, uint32_t _width, uint32_t _height,
    uint32_t _screen_refresh_rate, uint32_t _rotation, bool _stretch,
    std::function<void(uint32_t, std::shared_ptr<google::protobuf::MessageLite>, bool)>
        send_message,
    std::function<void()> _switch_stretch, std::function<void()> _reset_pipeline)
    : encode_codec(encode)
    , decode_codec(decode)
    , width(_width)
    , height(_height)
    , screen_refresh_rate(_screen_refresh_rate)
    , rotation(_rotation)
    , stretch(_stretch)
    , send_message_to_host(send_message)
    , switch_stretch(_switch_stretch)
    , reset_pipeline(_reset_pipeline) {
    status_color = -1;
}

bool DecodeRenderPipeline::Params::validate() const {
    if (encode_codec == lt::VideoCodecType::Unknown ||
        decode_codec == lt::VideoCodecType::Unknown || sdl == nullptr ||
        send_message_to_host == nullptr) {
        return false;
    }
    else {
        return true;
    }
}

std::unique_ptr<DecodeRenderPipeline> DecodeRenderPipeline::create(const Params& params) {
    if (!params.validate()) {
        LOG(FATAL) << "Create DecodeRenderPipeline failed: invalid parameter";
        return nullptr;
    }
    auto pipeline = VDRPipeline2::create(params);
    if (pipeline != nullptr) {
        return pipeline;
    }
    return VDRPipeline::create(params);
}

} // namespace video

} // namespace lt
