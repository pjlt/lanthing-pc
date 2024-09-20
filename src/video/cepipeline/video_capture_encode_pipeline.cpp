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
#include <deque>
#include <future>
#include <unordered_map>

#include <google/protobuf/message_lite.h>

#include <ltproto/client2worker/change_streaming_params.pb.h>
#include <ltproto/client2worker/cursor_info.pb.h>
#include <ltproto/client2worker/request_keyframe.pb.h>
#include <ltproto/ltproto.h>
#include <ltproto/worker2service/network_changed.pb.h>
#include <ltproto/worker2service/reconfigure_video_encoder.pb.h>

#include <ltlib/logging.h>
#include <ltlib/system.h>
#include <ltlib/threads.h>

#include <video/capturer/video_capturer.h>
#include <video/encoder/video_encoder.h>

namespace {

void addHistory(std::deque<int64_t>& history) {
    int64_t now = ltlib::steady_now_us();
    const int64_t kOneSecond = 1'000'000;
    const int64_t kOneSecondBefore = now - kOneSecond;
    history.push_back(now);
    while (!history.empty()) {
        if (history.front() < kOneSecondBefore) {
            history.pop_front();
        }
        else {
            break;
        }
    }
}

ltproto::client2worker::CursorInfo_CursorDataType toProtobuf(lt::video::CursorFormat format) {
    switch (format) {
    case lt::video::CursorFormat::MonoChrome:
        return ltproto::client2worker::CursorInfo_CursorDataType_MonoChrome;
    case lt::video::CursorFormat::Color:
        return ltproto::client2worker::CursorInfo_CursorDataType_Color;
    case lt::video::CursorFormat::MaskedColor:
        return ltproto::client2worker::CursorInfo_CursorDataType_MaskedColor;
    default:
        // 光标协议设计得太早, 导致这些不合理的地方
        LOG(FATAL) << "Unknown CursorFormat " << (int)format;
        return ltproto::client2worker::CursorInfo_CursorDataType_MonoChrome;
    }
}

} // namespace

namespace lt {

namespace video {

class VCEPipeline2 : public CaptureEncodePipeline {
public:
    static std::unique_ptr<VCEPipeline2> create(const CaptureEncodePipeline::Params& params);
    ~VCEPipeline2() override;
    bool start() override;
    void stop() override;
    VideoCodecType codec() const override;
    bool defaultOutput() override;

protected:
    VCEPipeline2();
};
VCEPipeline2::VCEPipeline2() = default;
VCEPipeline2::~VCEPipeline2() = default;
bool VCEPipeline2::start() {
    return false;
}
void VCEPipeline2::stop() {}
VideoCodecType VCEPipeline2::codec() const {
    return VideoCodecType::Unknown;
}
bool VCEPipeline2::defaultOutput() {
    return true;
}
// #if !defined(LT_WINDOWS)
std::unique_ptr<VCEPipeline2> VCEPipeline2::create(const CaptureEncodePipeline::Params&) {
    return nullptr;
}
// #endif // LT_WINDOWS

class VCEPipeline : public CaptureEncodePipeline {
public:
    static std::unique_ptr<VCEPipeline> create(const CaptureEncodePipeline::Params& params);
    ~VCEPipeline() override;
    bool start() override;
    void stop() override;
    VideoCodecType codec() const override;
    bool defaultOutput() override;

private:
    VCEPipeline(const CaptureEncodePipeline::Params& params);
    bool init();
    void mainLoop(const std::function<void()>& i_am_alive, std::promise<bool>& start_promise);
    void loadSystemCursor();
    bool registerHandlers();
    void consumeTasks();
    void captureAndSendCursor();
    void captureAndSendVideoFrame();
    auto resolutionChanged() -> std::optional<ltlib::DisplayOutputDesc>;
    void sendChangeStreamingParams(ltlib::DisplayOutputDesc desc);
    bool shouldEncodeFrame();
    auto getDxgiCursorInfo() -> std::shared_ptr<google::protobuf::MessageLite>;
    auto getWin32CursorInfo() -> std::shared_ptr<google::protobuf::MessageLite>;

    // 从service收到的消息
    void onReconfigure(std::shared_ptr<google::protobuf::MessageLite> msg);
    void onRequestKeyframe(std::shared_ptr<google::protobuf::MessageLite> msg);
    void onNetworkEvent(std::shared_ptr<google::protobuf::MessageLite> msg);

private:
    uint32_t width_;
    uint32_t height_;
    uint32_t client_refresh_rate_;
    uint32_t max_fps_;
    uint32_t target_fps_;
    uint32_t max_bps_;
    ltlib::Monitor monitor_;
    std::function<bool(uint32_t, const MessageHandler&)> register_message_handler_;
    std::function<bool(uint32_t, const std::shared_ptr<google::protobuf::MessageLite>&)>
        send_message_;
    std::vector<VideoCodecType> client_supported_codecs_;
    std::unique_ptr<ltlib::BlockingThread> thread_;
    std::unique_ptr<Capturer> capturer_;
    std::unique_ptr<Encoder> encoder_;
    uint64_t frame_no_ = 0;
    std::atomic<bool> stoped_{true};
    std::unique_ptr<std::promise<void>> stop_promise_;
    std::mutex mutex_;
    std::vector<std::function<void()>> tasks_;
    bool manual_bitrate_ = false;
    std::map<HCURSOR, int32_t> cursors_;
    bool get_win32_cursor_failed_ = false;
    std::deque<int64_t> capture_history_;
    int64_t last_encode_time_us_ = 0;
    bool half_fps_ = false;
    std::map<std::string, int32_t> cursors_map_;
    int32_t latest_cursor_id_ = 0;
};

VCEPipeline::VCEPipeline(const CaptureEncodePipeline::Params& params)
    : width_{params.width}
    , height_{params.height}
    , client_refresh_rate_{params.client_refresh_rate}
    , max_fps_{params.client_refresh_rate}
    , target_fps_{params.client_refresh_rate}
    , max_bps_{(params.max_mbps == 0 || params.max_mbps > 100) ? (100 * 1000 * 1000)
                                                               : (params.max_mbps * 1000 * 1000)}
    , monitor_{params.monitor}
    , register_message_handler_{params.register_message_handler}
    , send_message_{params.send_message}
    , client_supported_codecs_{params.codecs} {}

std::unique_ptr<VCEPipeline> VCEPipeline::create(const CaptureEncodePipeline::Params& params) {
    std::unique_ptr<VCEPipeline> pipeline{new VCEPipeline(params)};
    if (pipeline->init()) {
        return pipeline;
    }
    else {
        return nullptr;
    }
}

VCEPipeline::~VCEPipeline() {
    stop();
}

bool VCEPipeline::init() {
    constexpr uint32_t k144FPS = 144;
    constexpr uint32_t k30FPS = 30;
    loadSystemCursor();
    if (!registerHandlers()) {
        return false;
    }
    auto capturer = Capturer::create(Capturer::Backend::Dxgi, monitor_);
    if (capturer == nullptr) {
        return false;
    }
    max_fps_ = std::max(
        std::min({static_cast<uint32_t>(monitor_.frequency), client_refresh_rate_, k144FPS}),
        k30FPS);
    target_fps_ = max_fps_;
    Encoder::InitParams encode_params{};
    encode_params.freq = max_fps_;
    encode_params.bitrate_bps = 4 * 1000 * 1000;
    encode_params.bitrate_bps = std::min(encode_params.bitrate_bps, max_bps_);
    if (monitor_.rotation == 90 || monitor_.rotation == 270) {
        encode_params.width = height_;
        encode_params.height = width_;
    }
    else {
        encode_params.width = width_;
        encode_params.height = height_;
    }
    encode_params.luid = capturer->luid();
    encode_params.device = capturer->device();
    encode_params.context = capturer->deviceContext();
    encode_params.vendor_id = capturer->vendorID();
    std::unique_ptr<Encoder> encoder;
    // try hard first
    for (auto codec : client_supported_codecs_) {
        if (codec == VideoCodecType::H264_420_SOFT) {
            encode_params.codec_type = VideoCodecType::H264_420;
            encode_params.freq = k30FPS;
        }
        else {
            encode_params.freq = max_fps_;
            encode_params.codec_type = codec;
        }
        encoder = Encoder::createHard(encode_params);
        if (encoder) {
            if (codec == VideoCodecType::H264_420_SOFT) {
                max_fps_ = k30FPS;
                target_fps_ = max_fps_;
            }
            break;
        }
    }
    // fallback
    if (encoder == nullptr) {
        max_fps_ = k30FPS;
        target_fps_ = max_fps_;
        encode_params.freq = max_fps_;
        for (auto codec : client_supported_codecs_) {
            encode_params.codec_type =
                codec == VideoCodecType::H264_420_SOFT ? VideoCodecType::H264_420 : codec;
            encoder = Encoder::createSoft(encode_params);
            if (encoder) {
                break;
            }
        }
    }
    if (encoder != nullptr) {
        if (!capturer->setCaptureFormat(encoder->captureFormat())) {
            return false;
        }
        encoder_ = std::move(encoder);
        capturer_ = std::move(capturer);
        return true;
    }
    return false;
}

bool VCEPipeline::start() {
    std::promise<bool> start_promise;
    thread_ = ltlib::BlockingThread::create(
        "lt_video_capture_encode", [this, &start_promise](const std::function<void()>& i_am_alive) {
            mainLoop(i_am_alive, start_promise);
        });
    return start_promise.get_future().get();
}

void VCEPipeline::stop() {
    // 即便stoped_是原子也不应该这么做，但这个程序似乎不会出现一个线程正在析构VideoCapture，另一个线程主动调stop()
    if (!stoped_ && stop_promise_ != nullptr) {
        stoped_ = true;
        stop_promise_->get_future().get();
    }
}

VideoCodecType VCEPipeline::codec() const {
    return encoder_->codecType();
}

bool VCEPipeline::defaultOutput() {
    if (capturer_) {
        return capturer_->defaultOutput();
    }
    else {
        return true;
    }
}

void VCEPipeline::mainLoop(const std::function<void()>& i_am_alive,
                           std::promise<bool>& start_promise) {
    if (!ltlib::setThreadDesktop()) {
        LOG(ERR) << "VCEPipeline::mainLoop setThreadDesktop failed";
        start_promise.set_value(false);
        return;
    }
    if (!capturer_->start()) {
        LOG(ERR) << "Start video capturer failed";
        start_promise.set_value(false);
        return;
    }
    start_promise.set_value(true);
    stop_promise_ = std::make_unique<std::promise<void>>();
    stoped_ = false;
    LOG(INFO) << "CaptureEncodePipeline start";
    while (!stoped_) {
        i_am_alive();
        // vblank后应该第一时间抓屏还是消费任务？
        consumeTasks();
        auto resolution = resolutionChanged();
        if (resolution.has_value()) {
            sendChangeStreamingParams(resolution.value());
            stoped_ = true;
            break;
        }
        capturer_->waitForVBlank();
        captureAndSendVideoFrame();
        captureAndSendCursor();
    }
    stop_promise_->set_value();
    LOG(INFO) << "CaptureEncodePipeline stoped";
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
        {ltype::kRequestKeyframe, std::bind(&VCEPipeline::onRequestKeyframe, this, ph::_1)},
        {ltype::kNetworkChanged, std::bind(&VCEPipeline::onNetworkEvent, this, ph::_1)}};
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
    auto msg = getDxgiCursorInfo();
    if (msg == nullptr) {
        msg = getWin32CursorInfo();
    }
    if (msg == nullptr) {
        return;
    }
    auto cursor = std::static_pointer_cast<ltproto::client2worker::CursorInfo>(msg);
    if (!cursor->data().empty()) {
        auto iter = cursors_map_.find(cursor->data());
        if (iter != cursors_map_.end()) {
            cursor->set_data_id(iter->second);
            cursor->clear_data();
        }
        else {
            latest_cursor_id_ += 1;
            cursors_map_.insert({cursor->data(), latest_cursor_id_});
            cursor->set_data_id(latest_cursor_id_);
        }
    }

    send_message_(ltproto::id(cursor), cursor);
}

void VCEPipeline::captureAndSendVideoFrame() {
    auto captured_frame = capturer_->capture();
    if (!captured_frame.has_value()) {
        return;
    }
    if (encoder_->doneFrame1()) {
        capturer_->doneWithFrame();
    }
    if (!shouldEncodeFrame()) {
        if (encoder_->doneFrame2()) {
            capturer_->doneWithFrame();
        }
        return;
    }
    auto encoded_frame = encoder_->encode(captured_frame.value());
    if (encoder_->doneFrame2()) {
        capturer_->doneWithFrame();
    }
    if (encoded_frame == nullptr) {
        return;
    }
    // TODO: 计算编码完成距离上一次vblank时间
    send_message_(ltproto::id(encoded_frame), encoded_frame);
}

std::optional<ltlib::DisplayOutputDesc> VCEPipeline::resolutionChanged() {
    ltlib::DisplayOutputDesc desc = ltlib::getDisplayOutputDesc(monitor_.name);
    if (desc.height != static_cast<int32_t>(height_) ||
        desc.width != static_cast<int32_t>(width_)) {
        LOGF(INFO, "The resolution has changed from {w:%u, h:%u} to {w:%d, h:%d}", width_, height_,
             desc.width, desc.height);
        return desc;
    }
    return std::nullopt;
}

// 当前只关注分辨率
void VCEPipeline::sendChangeStreamingParams(ltlib::DisplayOutputDesc desc) {
    auto msg = std::make_shared<ltproto::client2worker::ChangeStreamingParams>();
    auto params = msg->mutable_params();
    params->set_video_width(desc.width);
    params->set_video_height(desc.height);
    params->set_screen_refresh_rate(desc.frequency);
    params->set_rotation(desc.rotation);
    send_message_(ltproto::id(msg), msg);
}

bool VCEPipeline::shouldEncodeFrame() {
    addHistory(capture_history_);
    const size_t capture_fps = capture_history_.size();
    const uint32_t target_fps = half_fps_ ? (target_fps_ / 2) : target_fps_;
    const uint32_t interval_us = 1'000'000 / target_fps;
    const int64_t now_us = ltlib::steady_now_us();
    if (capture_fps > target_fps + 2) {
        int64_t last_target_encode_time_us = now_us - now_us % interval_us;
        if (last_encode_time_us_ >= last_target_encode_time_us) {
            return false;
        }
    }
    last_encode_time_us_ = now_us;
    return true;
}

std::shared_ptr<google::protobuf::MessageLite> VCEPipeline::getDxgiCursorInfo() {
    auto info = capturer_->cursorInfo();
    if (!info.has_value()) {
        return nullptr;
    }
    auto msg = std::make_shared<ltproto::client2worker::CursorInfo>();
    msg->set_visible(info->visible);
    msg->set_x(info->x);
    msg->set_y(info->y);
    msg->set_w(ltlib::getScreenWidth());
    msg->set_h(ltlib::getScreenHeight());
    if (!info->data.empty()) {
        msg->set_cursor_w(info->w);
        msg->set_cursor_h(info->h);
        msg->set_hot_x(info->hot_x);
        msg->set_hot_y(info->hot_y);
        msg->set_pitch(info->pitch);
        msg->set_type(toProtobuf(info->format));
        msg->set_data(info->data.data(), info->data.size());
    }
    return msg;
}

static CursorFormat getCursorDataFromHcursor(HCURSOR hcursor, std::vector<uint8_t>& cursor_data,
                                             uint32_t& w, uint32_t& h, uint16_t& hot_x,
                                             uint16_t& hot_y, uint32_t& pitch) {
    int32_t color_width = 0;
    int32_t color_height = 0;
    int32_t color_bits_pixel = 0;
    int32_t mask_width = 0;
    int32_t mask_height = 0;
    int32_t mask_bits_pixel = 0;
    std::vector<uint8_t> color_data;
    std::vector<uint8_t> mask_data;
    ICONINFO iconinfo{};
    BOOL ret = GetIconInfo(hcursor, &iconinfo);
    if (ret != TRUE) {
        LOG(ERR) << "GetIconInfo failed: 0x" << std::hex << GetLastError();
        return CursorFormat::Unknown;
    }
    hot_x = static_cast<uint16_t>(iconinfo.xHotspot);
    hot_y = static_cast<uint16_t>(iconinfo.yHotspot);
    if (iconinfo.hbmColor) {
        BITMAP bmp{};
        GetObjectA(iconinfo.hbmColor, sizeof(BITMAP), &bmp);
        if (!bmp.bmWidthBytes || !bmp.bmHeight) {
            return CursorFormat::Unknown;
        }

        color_data.resize(bmp.bmWidthBytes * bmp.bmHeight);
        if (!GetBitmapBits(iconinfo.hbmColor, bmp.bmWidthBytes * bmp.bmHeight, color_data.data())) {
            return CursorFormat::Unknown;
        }

        color_width = bmp.bmWidth;
        color_height = bmp.bmHeight;
        color_bits_pixel = bmp.bmBitsPixel;
    }
    if (iconinfo.hbmMask) {
        BITMAP bmp{};
        GetObjectA(iconinfo.hbmMask, sizeof(BITMAP), &bmp);
        if (!bmp.bmWidthBytes || !bmp.bmHeight) {
            return CursorFormat::Unknown;
        }

        mask_data.resize(bmp.bmWidthBytes * bmp.bmHeight);
        if (!GetBitmapBits(iconinfo.hbmMask, bmp.bmWidthBytes * bmp.bmHeight, mask_data.data())) {
            return CursorFormat::Unknown;
        }

        mask_width = bmp.bmWidth;
        mask_height = bmp.bmHeight;
        mask_bits_pixel = bmp.bmBitsPixel;
    }
    if (iconinfo.hbmColor) {
        DeleteObject(iconinfo.hbmColor);
    }
    if (iconinfo.hbmMask) {
        DeleteObject(iconinfo.hbmMask);
    }
    if (color_data.empty() && !mask_data.empty() && mask_bits_pixel == 1) {
        w = mask_width;
        h = mask_height;
        pitch = static_cast<uint32_t>(mask_data.size() / h);
        cursor_data = std::move(mask_data);
        return CursorFormat::MonoChrome;
    }
    else if (!color_data.empty() && !mask_data.empty() && mask_bits_pixel == 32) { // ???
        w = color_width;
        h = color_height;
        pitch = color_width * 4;
        cursor_data.resize(color_data.size() + mask_data.size());
        memcpy(cursor_data.data(), color_data.data(), color_data.size());
        memcpy(cursor_data.data() + color_data.size(), mask_data.data(), mask_data.size());
        return CursorFormat::MaskedColor;
    }
    else if (!color_data.empty()) {
        w = color_width;
        h = color_height;
        pitch = color_width * 4;
        cursor_data = std::move(color_data);
        return CursorFormat::Color;
    }
    LOG(WARNING) << "getCursorDataFromHcursor failed, color size:" << color_data.size()
                 << ", mask size:" << mask_data.size() << ", mask_bits_pixel:" << mask_bits_pixel;
    return CursorFormat::Unknown;
}

std::shared_ptr<google::protobuf::MessageLite> VCEPipeline::getWin32CursorInfo() {
    auto msg = std::make_shared<ltproto::client2worker::CursorInfo>();
    msg->set_w(ltlib::getScreenWidth());
    msg->set_h(ltlib::getScreenHeight());
    CURSORINFO pci{};
    POINT pos{};
    DWORD error1 = 0;
    DWORD error2 = 0;
    pci.cbSize = sizeof(pci);
    if (GetCursorInfo(&pci)) {
        get_win32_cursor_failed_ = false;
        msg->set_x(pci.ptScreenPos.x);
        msg->set_y(pci.ptScreenPos.y);
        msg->set_visible(pci.flags != 0);
        auto iter = cursors_.find(pci.hCursor);
        if (iter != cursors_.end()) {
            msg->set_preset(
                static_cast<ltproto::client2worker::CursorInfo_PresetCursor>(iter->second));
        }
        std::vector<uint8_t> cursor_data;
        uint16_t hot_x = 0, hot_y = 0;
        uint32_t cursor_w = 0, cursor_h = 0, pitch = 0;
        CursorFormat format = getCursorDataFromHcursor(pci.hCursor, cursor_data, cursor_w, cursor_h,
                                                       hot_x, hot_y, pitch);
        if (format == CursorFormat::Unknown) {
            // some log
        }
        else {
            msg->set_data(cursor_data.data(), cursor_data.size());
            msg->set_x(pci.ptScreenPos.x - hot_x);
            msg->set_y(pci.ptScreenPos.y - hot_y);
            msg->set_hot_x(hot_x);
            msg->set_hot_y(hot_y);
            msg->set_type(toProtobuf(format));
            msg->set_cursor_h(cursor_h);
            msg->set_cursor_w(cursor_w);
            msg->set_pitch(pitch);
            return msg;
        }
    }
    else {
        error1 = GetLastError();
    }
    ltlib::setThreadDesktop();
    if (GetCursorPos(&pos)) {
        get_win32_cursor_failed_ = false;
        msg->set_preset(ltproto::client2worker::CursorInfo_PresetCursor_Arrow);
        msg->set_x(pos.x);
        msg->set_y(pos.y);
        msg->set_visible(true);
        // send_message_(ltproto::id(msg), msg);
        return msg;
    }
    else {
        error2 = GetLastError();
    }

    // 这个标志位是为了只打一次这个日志
    if (!get_win32_cursor_failed_) {
        // 这么写获取不到错误码，但是要获得错误码的写法很丑
        LOGF(ERR, "GetCursorInfo=>%u and GetCursorPos=>%u", error1, error2);
    }
    get_win32_cursor_failed_ = true;
    return nullptr;
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
        Encoder::ReconfigureParams params{};
        bool changed = false;
        if (msg->has_bitrate_bps()) {
            LOG(DEBUG) << "Set bitrate " << msg->bitrate_bps();
            params.bitrate_bps = msg->bitrate_bps();
            if (!manual_bitrate_) {
                params.bitrate_bps = std::min(params.bitrate_bps.value(), max_bps_);
            }
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

void VCEPipeline::onNetworkEvent(std::shared_ptr<google::protobuf::MessageLite> _msg) {
    std::lock_guard lock{mutex_};
    tasks_.push_back([this, _msg] {
        auto msg = std::static_pointer_cast<ltproto::worker2service::NetworkChanged>(_msg);
        switch (msg->network_event()) {
        case ltproto::worker2service::NetworkChanged_NetworkEvent_LinkTypeRelay:
            half_fps_ = true;
            break;
        case ltproto::worker2service::NetworkChanged_NetworkEvent_LinkTypeLan:
        case ltproto::worker2service::NetworkChanged_NetworkEvent_LinkTypeWan:
        case ltproto::worker2service::NetworkChanged_NetworkEvent_LinkTypeTCP:
            half_fps_ = false;
            break;
        default:
            LOG(WARNING) << "Received unknown NetworkEvent " << (int)msg->network_event();
            break;
        }
    });
}

std::unique_ptr<CaptureEncodePipeline> CaptureEncodePipeline::create(const Params& params) {
    if (params.send_message == nullptr || params.register_message_handler == nullptr ||
        params.width == 0 || params.height == 0 || params.codecs.empty() ||
        params.client_refresh_rate == 0) {
        LOG(FATAL) << "Create CaptureEncodePipeline failed, invalid parameters";
        return nullptr;
    }
    Params params2 = params;
    params2.codecs.clear();
    bool h264_420_inserted = false;
    for (auto codec : params.codecs) {
        switch (codec) {
        case VideoCodecType::H264_420:
            if (!h264_420_inserted) {
                h264_420_inserted = true;
            }
            params2.codecs.push_back(codec);
            break;
        case VideoCodecType::H264_420_SOFT:
            if (!h264_420_inserted) {
                h264_420_inserted = true;
            }
            params2.codecs.push_back(VideoCodecType::H264_420);
            break;
        case VideoCodecType::H265_420:
        case VideoCodecType::H264_444:
        case VideoCodecType::H265_444:
            params2.codecs.push_back(codec);
            break;
        default:
            break;
        }
    }
    if (params2.codecs.empty()) {
        // fatal error
        LOG(ERR) << "Init VideoCaptureEncodePipeline failed: only support avc and hevc";
        return nullptr;
    }
    auto pipeline = VCEPipeline2::create(params2);
    if (pipeline != nullptr) {
        return pipeline;
    }
    return VCEPipeline::create(params2);
}

} // namespace video

} // namespace lt
