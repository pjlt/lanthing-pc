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

#include "video_capture_encode_pipeline.h"

#include <Windows.h>
#include <winuser.h>

#include <atomic>
#include <cstdint>
#include <future>

#include <google/protobuf/message_lite.h>

#include <ltproto/client2worker/cursor_info.pb.h>
#include <ltproto/client2worker/request_keyframe.pb.h>
#include <ltproto/ltproto.h>
#include <ltproto/worker2service/reconfigure_video_encoder.pb.h>

#include <ltlib/logging.h>
#include <ltlib/system.h>
#include <ltlib/threads.h>

#include <graphics/capturer/video_capturer.h>
#include <graphics/encoder/video_encoder.h>

namespace lt {

class VCEPipeline {
public:
    VCEPipeline(const VideoCaptureEncodePipeline::Params& params);
    ~VCEPipeline();
    bool init();
    void start();
    void stop();
    VideoCodecType codec() const;

private:
    void mainLoop(const std::function<void()>& i_am_alive);
    void loadSystemCursor();
    bool registerHandlers();
    void consumeTasks();
    void captureAndSendCursor();
    void captureAndSendVideoFrame();

    // 从service收到的消息
    void onReconfigure(std::shared_ptr<google::protobuf::MessageLite> msg);
    void onRequestKeyframe(std::shared_ptr<google::protobuf::MessageLite> msg);

private:
    uint32_t width_;
    uint32_t height_;
    std::function<bool(uint32_t, const MessageHandler&)> register_message_handler_;
    std::function<bool(uint32_t, const std::shared_ptr<google::protobuf::MessageLite>&)>
        send_message_;
    std::vector<VideoCodecType> client_supported_codecs_;
    std::unique_ptr<ltlib::BlockingThread> thread_;
    std::unique_ptr<VideoCapturer> capturer_;
    std::unique_ptr<VideoEncoder> encoder_;
    uint64_t frame_no_ = 0;
    std::atomic<bool> stoped_{true};
    std::unique_ptr<std::promise<void>> stop_promise_;
    VideoCodecType codec_type_ = VideoCodecType::Unknown;
    std::mutex mutex_;
    std::vector<std::function<void()>> tasks_;
    bool manual_bitrate_ = false;
    std::map<HCURSOR, int32_t> cursors_;
    bool get_cursor_failed_ = false;
};

VCEPipeline::VCEPipeline(const VideoCaptureEncodePipeline::Params& params)
    : width_{params.width}
    , height_{params.height}
    , register_message_handler_{params.register_message_handler}
    , send_message_{params.send_message}
    , client_supported_codecs_{params.codecs} {}

VCEPipeline::~VCEPipeline() {
    stop();
}

bool VCEPipeline::init() {
    loadSystemCursor();
    if (!registerHandlers()) {
        return false;
    }
    auto capturer = VideoCapturer::create(VideoCapturer::Backend::Dxgi);
    if (capturer == nullptr) {
        return false;
    }
    VideoEncoder::InitParams encode_params{};
    encode_params.bitrate_bps = 4 * 1024 * 1024;
    encode_params.width = width_;
    encode_params.height = height_;
    encode_params.luid = capturer->luid();
    encode_params.device = capturer->device();
    encode_params.context = capturer->deviceContext();
    encode_params.vendor_id = capturer->vendorID();
    std::unique_ptr<VideoEncoder> encoder;
    for (auto codec : client_supported_codecs_) {
        encode_params.codec_type = codec;
        encoder = VideoEncoder::create(encode_params);
        if (encoder) {
            codec_type_ = codec;
            break;
        }
    }
    if (encoder == nullptr) {
        return false;
    }
    encoder_ = std::move(encoder);
    capturer_ = std::move(capturer);
    return true;
}

void VCEPipeline::start() {
    thread_ = ltlib::BlockingThread::create(
        "video_capture_encode",
        [this](const std::function<void()>& i_am_alive) { mainLoop(i_am_alive); });
}

void VCEPipeline::stop() {
    // 即便stoped_是原子也不应该这么做，但这个程序似乎不会出现一个线程正在析构VideoCapture，另一个线程主动调stop()
    if (!stoped_) {
        stoped_ = true;
        stop_promise_->get_future().get();
    }
}

VideoCodecType VCEPipeline::codec() const {
    return codec_type_;
}

void VCEPipeline::mainLoop(const std::function<void()>& i_am_alive) {
    ltlib::setThreadDesktop();
    stop_promise_ = std::make_unique<std::promise<void>>();
    stoped_ = false;
    LOG(INFO) << "VideoCaptureEncodePipeline start";
    while (!stoped_) {
        i_am_alive();
        // vblank后应该第一时间抓屏还是消费任务？
        consumeTasks();
        capturer_->waitForVBlank();
        captureAndSendVideoFrame();
        // 光标逻辑是否要放到其它线程捕获？先在同一线程做，后面做优化再来测，用数据说话
        captureAndSendCursor();
    }
    stop_promise_->set_value();
}

void VCEPipeline::loadSystemCursor() { // 释放?
    cursors_[LoadCursorA(nullptr, IDC_ARROW)] =
        ltproto::client2worker::CursorInfo_PresetCursor_Arrow;
    cursors_[LoadCursorA(nullptr, IDC_IBEAM)] =
        ltproto::client2worker::CursorInfo_PresetCursor_Ibeam;
    cursors_[LoadCursorA(nullptr, IDC_WAIT)] = ltproto::client2worker::CursorInfo_PresetCursor_Wait;
    cursors_[LoadCursorA(nullptr, IDC_CROSS)] =
        ltproto::client2worker::CursorInfo_PresetCursor_Cross;
    cursors_[LoadCursorA(nullptr, IDC_SIZENWSE)] =
        ltproto::client2worker::CursorInfo_PresetCursor_SizeNwse;
    cursors_[LoadCursorA(nullptr, IDC_SIZENESW)] =
        ltproto::client2worker::CursorInfo_PresetCursor_SizeNesw;
    cursors_[LoadCursorA(nullptr, IDC_SIZEWE)] =
        ltproto::client2worker::CursorInfo_PresetCursor_SizeWe;
    cursors_[LoadCursorA(nullptr, IDC_SIZENS)] =
        ltproto::client2worker::CursorInfo_PresetCursor_SizeNs;
    cursors_[LoadCursorA(nullptr, IDC_SIZEALL)] =
        ltproto::client2worker::CursorInfo_PresetCursor_SizeAll;
    cursors_[LoadCursorA(nullptr, IDC_NO)] = ltproto::client2worker::CursorInfo_PresetCursor_No;
    cursors_[LoadCursorA(nullptr, IDC_HAND)] = ltproto::client2worker::CursorInfo_PresetCursor_Hand;
}

bool VCEPipeline::registerHandlers() {
    namespace ltype = ltproto::type;
    namespace ph = std::placeholders;
    const std::pair<uint32_t, MessageHandler> handlers[] = {
        {ltype::kReconfigureVideoEncoder, std::bind(&VCEPipeline::onReconfigure, this, ph::_1)},
        {ltype::kRequestKeyframe, std::bind(&VCEPipeline::onRequestKeyframe, this, ph::_1)}};
    for (auto& handler : handlers) {
        if (!register_message_handler_(handler.first, handler.second)) {
            return false;
        }
    }
    return true;
}

void VCEPipeline::consumeTasks() {
    std::vector<std::function<void()>> tasks;
    {
        std::lock_guard lock{mutex_};
        tasks = std::move(tasks_);
    }
    for (auto& task : tasks) {
        task();
    }
}

void VCEPipeline::captureAndSendCursor() {
    auto msg = std::make_shared<ltproto::client2worker::CursorInfo>();
    msg->set_w(ltlib::getScreenWidth());
    msg->set_h(ltlib::getScreenHeight());
    CURSORINFO pci{};
    POINT pos{};
    DWORD error1 = 0;
    DWORD error2 = 0;
    pci.cbSize = sizeof(pci);
    if (GetCursorInfo(&pci)) {
        get_cursor_failed_ = false;
        msg->set_x(pci.ptScreenPos.x);
        msg->set_y(pci.ptScreenPos.y);
        msg->set_visible(pci.flags != 0);
        auto iter = cursors_.find(pci.hCursor);
        if (iter == cursors_.end()) {
            msg->set_preset(ltproto::client2worker::CursorInfo_PresetCursor_Arrow);
        }
        else {
            msg->set_preset(
                static_cast<ltproto::client2worker::CursorInfo_PresetCursor>(iter->second));
        }
        send_message_(ltproto::id(msg), msg);
        return;
    }
    else {
        error1 = GetLastError();
    }
    ltlib::setThreadDesktop();
    if (GetCursorPos(&pos)) {
        get_cursor_failed_ = false;
        msg->set_preset(ltproto::client2worker::CursorInfo_PresetCursor_Arrow);
        msg->set_x(pos.x);
        msg->set_y(pos.y);
        msg->set_visible(true);
        send_message_(ltproto::id(msg), msg);
        return;
    }
    else {
        error2 = GetLastError();
    }

    // 这个标志位是为了只打一次这个日志
    if (!get_cursor_failed_) {
        // 这么写获取不到错误码，但是要获得错误码的写法很丑
        LOGF(ERR, "GetCursorInfo=>%u and GetCursorPos=>%u", error1, error2);
    }
    get_cursor_failed_ = true;
}

void VCEPipeline::captureAndSendVideoFrame() {
    auto captured_frame = capturer_->capture();
    if (!captured_frame.has_value()) {
        return;
    }
    capturer_->doneWithFrame();
    auto encoded_frame = encoder_->encode(captured_frame.value());
    if (encoded_frame == nullptr) {
        return;
    }
    // TODO: 计算编码完成距离上一次vblank时间
    send_message_(ltproto::id(encoded_frame), encoded_frame);
}

void VCEPipeline::onReconfigure(std::shared_ptr<google::protobuf::MessageLite> _msg) {
    std::lock_guard lock{mutex_};
    tasks_.push_back([_msg, this]() {
        auto msg = std::static_pointer_cast<ltproto::worker2service::ReconfigureVideoEncoder>(_msg);
        // 如果设置了手动码率，只接受带有trigger的Reconfigure消息
        if (manual_bitrate_ && !msg->has_trigger()) {
            return;
        }
        if (msg->has_trigger()) {
            switch (msg->trigger()) {
            case ltproto::worker2service::ReconfigureVideoEncoder_Trigger_TurnOnAuto:
                LOG(DEBUG) << "Turn on auto bitrate";
                manual_bitrate_ = false;
                return;
            case ltproto::worker2service::ReconfigureVideoEncoder_Trigger_TurnOffAuto:
                LOG(DEBUG) << "Turn off auto bitrate";
                manual_bitrate_ = true;
                break;
            default:
                LOG(WARNING) << "ReconfigureVideoEncoder has wrong trigger value: "
                             << (int)msg->trigger();
                break;
            }
        }
        VideoEncoder::ReconfigureParams params{};
        bool changed = false;
        if (msg->has_bitrate_bps()) {
            LOG(DEBUG) << "Set bitrate " << msg->bitrate_bps();
            params.bitrate_bps = msg->bitrate_bps();
            changed = true;
        }
        if (msg->has_fps()) {
            params.fps = msg->fps();
            changed = true;
        }
        if (changed) {
            encoder_->reconfigure(params);
        }
    });
}

void VCEPipeline::onRequestKeyframe(std::shared_ptr<google::protobuf::MessageLite> msg) {
    (void)msg;
    std::lock_guard lock{mutex_};
    tasks_.push_back([this] { encoder_->requestKeyframe(); });
}

std::unique_ptr<VideoCaptureEncodePipeline>
VideoCaptureEncodePipeline::create(const Params& params) {
    if (params.send_message == nullptr || params.register_message_handler == nullptr ||
        params.width == 0 || params.height == 0) {
        LOG(FATAL) << "Create VideoCaptureEncodePipeline failed, invalid parameters";
        return nullptr;
    }
    auto impl = std::make_shared<VCEPipeline>(params);
    if (!impl->init()) {
        return nullptr;
    }
    std::unique_ptr<VideoCaptureEncodePipeline> pipeline{new VideoCaptureEncodePipeline};
    pipeline->impl_ = std::move(impl);
    return pipeline;
}

void VideoCaptureEncodePipeline::start() {
    impl_->start();
}

void VideoCaptureEncodePipeline::stop() {
    impl_->stop();
}

VideoCodecType VideoCaptureEncodePipeline::codec() const {
    return impl_->codec();
}

} // namespace lt