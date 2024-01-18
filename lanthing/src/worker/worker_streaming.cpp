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

#include "worker_streaming.h"

#include <ltproto/client2worker/audio_data.pb.h>
#include <ltproto/client2worker/change_streaming_params.pb.h>
#include <ltproto/client2worker/change_streaming_params_ack.pb.h>
#include <ltproto/common/keep_alive_ack.pb.h>
#include <ltproto/common/streaming_params.pb.h>
#include <ltproto/ltproto.h>
#include <ltproto/worker2service/start_working.pb.h>
#include <ltproto/worker2service/start_working_ack.pb.h>

#include <lt_constants.h>
#include <ltlib/logging.h>
#include <ltlib/system.h>
#include <ltlib/times.h>

namespace {

lt::VideoCodecType toLtrtc(ltproto::common::VideoCodecType codec) {
    switch (codec) {
    case ltproto::common::AVC:
        return lt::VideoCodecType::H264_420;
    case ltproto::common::HEVC:
        return lt::VideoCodecType::H265_420;
    case ltproto::common::AVC_444:
        return lt::VideoCodecType::H264_444;
    case ltproto::common::HEVC_444:
        return lt::VideoCodecType::H265_444;
    case ltproto::common::AV1:
        return lt::VideoCodecType::AV1;
    default:
        return lt::VideoCodecType::Unknown;
    }
}

ltproto::common::VideoCodecType toProtobuf(lt::VideoCodecType codec_type) {
    switch (codec_type) {
    case lt::VideoCodecType::H264_420:
        return ltproto::common::VideoCodecType::AVC;
    case lt::VideoCodecType::H265_420:
        return ltproto::common::VideoCodecType::HEVC;
    case lt::VideoCodecType::H264_444:
        return ltproto::common::VideoCodecType::AVC_444;
    case lt::VideoCodecType::H265_444:
        return ltproto::common::VideoCodecType::HEVC_444;
    case lt::VideoCodecType::AV1:
        return ltproto::common::VideoCodecType::AV1;
    default:
        return ltproto::common::VideoCodecType::UnknownVCT;
    }
}

} // namespace

namespace lt {

namespace worker {

std::unique_ptr<WorkerStreaming>
WorkerStreaming::create(std::map<std::string, std::string> options) {
    if (options.find("-width") == options.end() || options.find("-height") == options.end() ||
        options.find("-freq") == options.end() || options.find("-codecs") == options.end() ||
        options.find("-name") == options.end() || options.find("-negotiate") == options.end() ||
        options.find("-mindex") == options.end()) {
        LOG(ERR) << "Parameter invalid";
        return nullptr;
    }
    Params params{};
    int32_t width = std::atoi(options["-width"].c_str());
    int32_t height = std::atoi(options["-height"].c_str());
    int32_t freq = std::atoi(options["-freq"].c_str());
    int32_t monitor_index = std::atoi(options["-mindex"].c_str());
    std::stringstream ss(options["-codecs"]);

    params.need_negotiate = std::atoi(options["-negotiate"].c_str()) != 0;
    params.name = options["-name"];
    if (params.name.empty()) {
        LOG(ERR) << "Parameter invalid: name";
        return nullptr;
    }
    if (width <= 0) {
        LOG(ERR) << "Parameter invalid: width " << width;
        return nullptr;
    }
    params.width = static_cast<uint32_t>(width);
    if (height <= 0) {
        LOG(ERR) << "Parameter invalid: height " << height;
        return nullptr;
    }
    params.height = static_cast<uint32_t>(height);
    if (freq <= 0) {
        LOG(ERR) << "Parameter invalid: freq " << freq;
        return nullptr;
    }
    params.refresh_rate = static_cast<uint32_t>(freq);
    if (monitor_index < 0 || monitor_index >= 10) {
        LOG(ERR) << "Parameter invalid: mindex " << monitor_index;
    }
    params.monitor_index = monitor_index;
    std::string codec_str;
    while (std::getline(ss, codec_str, ',')) {
        VideoCodecType codec = videoCodecType(codec_str.c_str());
        if (codec != VideoCodecType::Unknown) {
            params.codecs.push_back(codec);
        }
    }
    if (params.codecs.empty()) {
        LOG(ERR) << "Parameter invalid: codecs";
        return nullptr;
    }

    std::unique_ptr<WorkerStreaming> worker{new WorkerStreaming{params}};
    if (!worker->init()) {
        return nullptr;
    }
    return worker;
}

WorkerStreaming::WorkerStreaming(const Params& params)
    : need_negotiate_{params.need_negotiate}
    , client_width_{params.width}
    , client_height_{params.height}
    , client_refresh_rate_{params.refresh_rate}
    , monitor_index_{params.monitor_index}
    , client_codec_types_{params.codecs}
    , pipe_name_{params.name}
    , last_time_received_from_service_{ltlib::steady_now_ms()} {}

WorkerStreaming::~WorkerStreaming() {
    recoverDisplaySettings();
    {
        std::lock_guard lock{mutex_};
        pipe_client_.reset();
        ioloop_.reset();
    }
}

int WorkerStreaming::wait() {
    return session_observer_->waitForChange();
}

bool WorkerStreaming::init() {
    monitors_ = ltlib::enumMonitors();
    if (monitors_.size() == 0) {
        LOG(ERR) << "There is no monitor";
        return false;
    }
    if (monitors_.size() - 1 < monitor_index_) {
        LOG(WARNING) << "Client requesting monitor " << monitor_index_ << ", but we only have "
                     << monitors_.size() << " monitors. Fallback to first monitor";
        monitor_index_ = 0;
    }
    for (auto& m : monitors_) {
        LOGF(INFO, "w:%d, h:%d, o:%d", m.width, m.height, m.rotation);
    }
    session_observer_ = SessionChangeObserver::create();
    if (session_observer_ == nullptr) {
        LOG(ERR) << "Create session observer failed";
        return false;
    }
    ioloop_ = ltlib::IOLoop::create();
    if (ioloop_ == nullptr) {
        LOG(ERR) << "Create IOLoop failed";
        return false;
    }
    if (!initPipeClient()) {
        LOG(ERR) << "Init pipe client failed";
        return false;
    }
#if 0 // 引入多屏支持后，协商变得很麻烦
    if (need_negotiate_) {
        if (!negotiateAllParameters()) {
            return false;
        }
    }
    else {
        negotiated_display_setting_.width = client_width_;
        negotiated_display_setting_.height = client_height_;
        negotiated_display_setting_.refrash_rate = client_refresh_rate_;
        if (!negotiateStreamParameters()) {
            return false;
        }
    }
#else
    if (!negotiateStreamParameters()) {
        return false;
    }
#endif

    namespace ltype = ltproto::type;
    namespace ph = std::placeholders;
    const std::pair<uint32_t, MessageHandler> handlers[] = {
        {ltype::kStartWorking, std::bind(&WorkerStreaming::onStartWorking, this, ph::_1)},
        {ltype::kStopWorking, std::bind(&WorkerStreaming::onStopWorking, this, ph::_1)},
        {ltype::kKeepAlive, std::bind(&WorkerStreaming::onKeepAlive, this, ph::_1)},
        {ltype::kChangeStreamingParamsAck,
         std::bind(&WorkerStreaming::onChangeStreamingParamsAck, this, ph::_1)},
        {ltype::kSwitchMonitor, std::bind(&WorkerStreaming::onSwitchMonitor, this, ph::_1)}};
    for (auto& handler : handlers) {
        if (!registerMessageHandler(handler.first, handler.second)) {
            LOG(FATAL) << "Register message handler(" << handler.first << ") failed";
        }
    }

    std::promise<void> promise;
    auto future = promise.get_future();
    thread_ = ltlib::BlockingThread::create(
        "main_thread", [this, &promise](const std::function<void()>& i_am_alive) {
            promise.set_value();
            mainLoop(i_am_alive);
        });
    future.get();
    ioloop_->postDelay(500 /*ms*/, std::bind(&WorkerStreaming::checkCimeout, this));
    return true;
} // namespace lt

bool WorkerStreaming::initPipeClient() {
    ltlib::Client::Params params{};
    params.stype = ltlib::StreamType::Pipe;
    params.ioloop = ioloop_.get();
    params.pipe_name = "\\\\?\\pipe\\" + pipe_name_;
    params.is_tls = false;
    params.on_closed = std::bind(&WorkerStreaming::onPipeDisconnected, this);
    params.on_connected = std::bind(&WorkerStreaming::onPipeConnected, this);
    params.on_message = std::bind(&WorkerStreaming::onPipeMessage, this, std::placeholders::_1,
                                  std::placeholders::_2);
    params.on_reconnecting = std::bind(&WorkerStreaming::onPipeReconnecting, this);
    pipe_client_ = ltlib::Client::create(params);
    return pipe_client_ != nullptr;
}

bool WorkerStreaming::saveAndChangeCurrentDisplaySettings(DisplaySettingNegotiator::Result result) {
    settings_ = ltlib::Settings::create(ltlib::Settings::Storage::Sqlite);
    if (settings_ == nullptr) {
        return false;
    }
    settings_->setInteger("old_screen_width", result.service.width);
    settings_->setInteger("old_screen_height", result.service.height);
    settings_->setInteger("old_screen_rate", result.service.refrash_rate);
    if (ltlib::changeDisplaySettings(result.negotiated.width, result.negotiated.height,
                                     result.negotiated.refrash_rate)) {
        LOGF(INFO, "Change display settings to {w:%u, h:%u, f:%u} success", result.negotiated.width,
             result.negotiated.height, result.negotiated.refrash_rate);
        return true;
    }
    else {
        LOGF(INFO, "Change display settings to {w:%u, h:%u, f:%u} failed", result.negotiated.width,
             result.negotiated.height, result.negotiated.refrash_rate);
        settings_ = nullptr;
        return false;
    }
}

void WorkerStreaming::recoverDisplaySettings() {
    if (settings_ == nullptr) {
        return;
    }
    auto width = settings_->getInteger("old_screen_width");
    auto height = settings_->getInteger("old_screen_height");
    auto rate = settings_->getInteger("old_screen_rate");
    if (!width.has_value() || !height.has_value() || !rate.has_value()) {
        LOG(WARNING) << "Get display setting from settings.db failed, won't recover";
        return;
    }
    settings_->deleteKey("old_screen_width");
    settings_->deleteKey("old_screen_height");
    settings_->deleteKey("old_screen_rate");
    uint32_t w = static_cast<uint32_t>(width.value());
    uint32_t h = static_cast<uint32_t>(height.value());
    uint32_t r = static_cast<uint32_t>(rate.value());
    if (ltlib::changeDisplaySettings(w, h, r)) {
        LOGF(INFO, "Recover display settings to {w:%u, h:%u, f:%u} success", w, h, r);
        return;
    }
    else {
        LOGF(INFO, "Recover display settings to {w:%u, h:%u, f:%u} failed", w, h, r);
        return;
    }
}

bool WorkerStreaming::negotiateAllParameters() {
    DisplaySetting client_display_setting{client_width_, client_height_, client_refresh_rate_};
    auto result = DisplaySettingNegotiator::negotiate(client_display_setting);
    if (result.negotiated.width == 0 || result.negotiated.height == 0) {
        LOG(WARNING) << "Negotiate display setting failed, fallback to default(width:1920, "
                        "height:1080, refresh_rate:60)";
        result.negotiated.width = 1920;
        result.negotiated.height = 1080;
        result.negotiated.refrash_rate = 60;
    }
    else if (result.negotiated.refrash_rate == 0) {
        LOG(WARNING) << "Negotiate display.refresh_rate failed, fallback to 60hz";
        result.negotiated.refrash_rate = 60;
    }
    LOGF(INFO, "Final negotiate display setting(width:%u, height:%u, refresh_rate:%u)",
         result.negotiated.width, result.negotiated.height, result.negotiated.refrash_rate);
    if (result.negotiated != result.service) {
        if (!saveAndChangeCurrentDisplaySettings(result)) {
            return false;
        }
    }
    // negotiated_display_setting_ = result.negotiated;
    if (negotiateStreamParameters() != 0) {
        return false;
    }
    return true;
}

int32_t WorkerStreaming::negotiateStreamParameters() {

    auto negotiated_params = std::make_shared<ltproto::common::StreamingParams>();

    lt::AudioCapturer::Params audio_params{};
#if LT_TRANSPORT_TYPE == LT_TRANSPORT_RTC
    audio_params.type = AudioCodecType::PCM;
#else
    audio_params.type = AudioCodecType::OPUS;
#endif
    audio_params.on_audio_data =
        std::bind(&WorkerStreaming::onCapturedAudioData, this, std::placeholders::_1);
    auto audio = AudioCapturer::create(audio_params);
    if (audio == nullptr) {
        return ltproto::ErrorCode::WorkerInitAudioFailed;
    }
    negotiated_params->set_audio_channels(audio->channels());
    negotiated_params->set_audio_sample_rate(audio->framesPerSec());

    lt::VideoCaptureEncodePipeline::Params video_params{};
    video_params.codecs = client_codec_types_;
    // video_params.width = negotiated_display_setting_.width;
    // video_params.height = negotiated_display_setting_.height;
    video_params.width = static_cast<uint32_t>(monitors_[monitor_index_].width);
    video_params.height = static_cast<uint32_t>(monitors_[monitor_index_].height);
    video_params.monitor = monitors_[monitor_index_];
    video_params.send_message = std::bind(&WorkerStreaming::sendPipeMessageFromOtherThread, this,
                                          std::placeholders::_1, std::placeholders::_2);
    video_params.register_message_handler =
        std::bind(&WorkerStreaming::registerMessageHandler, this, std::placeholders::_1,
                  std::placeholders::_2);
    auto video = lt::VideoCaptureEncodePipeline::create(video_params);
    if (video == nullptr) {
        LOGF(ERR, "Create VideoCaptureEncodePipeline failed");
        return ltproto::ErrorCode::WrokerInitVideoFailed;
    }
    negotiated_params->set_enable_driver_input(false);
    negotiated_params->set_enable_gamepad(false);
    // negotiated_params->set_screen_refresh_rate(negotiated_display_setting_.refrash_rate);
    // negotiated_params->set_video_width(negotiated_display_setting_.width);
    // negotiated_params->set_video_height(negotiated_display_setting_.height);
    negotiated_params->set_screen_refresh_rate(60); // 假的
    negotiated_params->set_video_width(video_params.width);
    negotiated_params->set_video_height(video_params.height);
    negotiated_params->add_video_codecs(toProtobuf(video->codec()));
    negotiated_params->set_rotation(monitors_[monitor_index_].rotation);
    negotiated_params->set_monitor_index(static_cast<int32_t>(monitor_index_));
    LOG(INFO) << "Negotiated video codec:" << toString(video->codec());

    negotiated_params_ = negotiated_params;
    video_ = std::move(video);
    audio_ = std::move(audio);
    return ltproto::ErrorCode::Success;
}

void WorkerStreaming::mainLoop(const std::function<void()>& i_am_alive) {
    LOG(INFO) << "Worker enter main loop";
    ioloop_->run(i_am_alive);
    LOG(INFO) << "Worker exit main loop";
}

void WorkerStreaming::stop(int exit_code) {
    session_observer_->stop(exit_code);
}

void WorkerStreaming::postTask(const std::function<void()>& task) {
    mutex_.lock_shared();
    if (ioloop_) {
        ioloop_->post(task);
    }
    mutex_.unlock_shared();
}

void WorkerStreaming::postDelayTask(int64_t delay_ms, const std::function<void()>& task) {
    mutex_.lock_shared();
    if (ioloop_) {
        ioloop_->postDelay(delay_ms, task);
    }
    mutex_.unlock_shared();
}

bool WorkerStreaming::registerMessageHandler(uint32_t type, const MessageHandler& handler) {
    auto [_, success] = msg_handlers_.insert({type, handler});
    if (!success) {
        LOG(ERR) << "Register message handler(" << type << ") failed";
        return false;
    }
    else {
        return true;
    }
}

void WorkerStreaming::dispatchServiceMessage(
    uint32_t type, const std::shared_ptr<google::protobuf::MessageLite>& msg) {
    auto iter = msg_handlers_.find(type);
    if (iter == msg_handlers_.cend() || iter->second == nullptr) {
        LOG(WARNING) << "Unknown message type: " << type;
        return;
    }
    iter->second(msg);
}

bool WorkerStreaming::sendPipeMessage(uint32_t type,
                                      const std::shared_ptr<google::protobuf::MessageLite>& msg) {
    if (!connected_to_service_) {
        return false;
    }
    return pipe_client_->send(type, msg);
}

// FIXME: 返回值
bool WorkerStreaming::sendPipeMessageFromOtherThread(
    uint32_t type, const std::shared_ptr<google::protobuf::MessageLite>& msg) {
    postTask([type, msg, this]() { sendPipeMessage(type, msg); });
    return true;
}

void WorkerStreaming::printStats() {}

void WorkerStreaming::checkCimeout() {
    constexpr int64_t kTimeout = 10'000;
    auto now = ltlib::steady_now_ms();
    if (now - last_time_received_from_service_ > kTimeout) {
        LOG(WARNING) << "No packet from service for " << now - last_time_received_from_service_
                     << "ms, worker exit.";
        stop(kExitCodeTimeout);
    }
    else {
        postDelayTask(500, [this]() { checkCimeout(); });
    }
}

void WorkerStreaming::updateInput() {
    if (input_) {
        input_->update();
    }
    postDelayTask(100, std::bind(&WorkerStreaming::updateInput, this));
}

void WorkerStreaming::onCapturedAudioData(
    std::shared_ptr<google::protobuf::MessageLite> audio_data) {
    postTask([this, audio_data]() { sendPipeMessage(ltproto::type::kAudioData, audio_data); });
}

void WorkerStreaming::onPipeMessage(uint32_t type,
                                    std::shared_ptr<google::protobuf::MessageLite> msg) {
    dispatchServiceMessage(type, msg);
}

void WorkerStreaming::onPipeDisconnected() {
    LOG(ERR) << "Disconnected from service, won't reconnect again";
    connected_to_service_ = false;
}

void WorkerStreaming::onPipeReconnecting() {
    LOG(INFO) << "Reconnecting to service...";
    connected_to_service_ = false;
}

void WorkerStreaming::onPipeConnected() {
    if (connected_to_service_) {
        LOG(FATAL) << "Logic error, connected to service twice";
    }
    else {
        LOG(INFO) << "Connected to service";
    }
    connected_to_service_ = true;
    // 连上第一时间，向service发送协商好的串流参数
    auto params = std::static_pointer_cast<ltproto::common::StreamingParams>(negotiated_params_);
    sendPipeMessage(ltproto::id(params), params);
}

void WorkerStreaming::onStartWorking(const std::shared_ptr<google::protobuf::MessageLite>& _msg) {
    auto msg = std::static_pointer_cast<ltproto::worker2service::StartWorking>(_msg);
    auto ack = std::make_shared<ltproto::worker2service::StartWorkingAck>();
    int32_t error_code = kExitCodeStartWorkingFailed;
    do {
        if (!video_->start()) {
            ack->set_err_code(ltproto::ErrorCode::WrokerInitVideoFailed);
            error_code = kExitCodeInitVideoFailed;
            break;
        }
        if (video_->defaultOutput()) {
            monitor_index_ = 0;
        }
        audio_->start();

        InputExecutor::Params input_params{};
        input_params.types = static_cast<uint8_t>(InputExecutor::Type::WIN32_MESSAGE) |
                             static_cast<uint8_t>(InputExecutor::Type::WIN32_DRIVER);
        input_params.monitor = monitors_[monitor_index_];
        input_params.screen_width = static_cast<uint32_t>(input_params.monitor.width);
        input_params.screen_height = static_cast<uint32_t>(input_params.monitor.height);
        input_params.register_message_handler =
            std::bind(&WorkerStreaming::registerMessageHandler, this, std::placeholders::_1,
                      std::placeholders::_2);
        input_params.send_message = std::bind(&WorkerStreaming::sendPipeMessageFromOtherThread,
                                              this, std::placeholders::_1, std::placeholders::_2);
        input_ = InputExecutor::create(input_params);
        if (input_ == nullptr) {
            ack->set_err_code(ltproto::ErrorCode::WorkerInitInputFailed);
            error_code = kExitCodeInitInputFailed;
            break;
        }
        ack->set_err_code(ltproto::ErrorCode::Success);
        postDelayTask(100, std::bind(&WorkerStreaming::updateInput, this));
    } while (false);
    for (const auto& handler : msg_handlers_) {
        ack->add_msg_type(handler.first);
    }
    sendPipeMessage(ltproto::id(ack), ack);
    if (ack->err_code() != ltproto::ErrorCode::Success) {
        if (video_) {
            video_->stop();
        }
        if (audio_) {
            audio_->stop();
        }
        input_ = nullptr;
        LOG(ERR) << "Start working failed, exit worker";
        postDelayTask(100, std::bind(&WorkerStreaming::stop, this, error_code));
    }
}

void WorkerStreaming::onStopWorking(const std::shared_ptr<google::protobuf::MessageLite>&) {
    LOG(INFO) << "Received StopWorking";
    stop(kExitCodeOK);
}

void WorkerStreaming::onKeepAlive(const std::shared_ptr<google::protobuf::MessageLite>&) {
    last_time_received_from_service_ = ltlib::steady_now_ms();
    auto ack = std::make_shared<ltproto::common::KeepAliveAck>();
    sendPipeMessage(ltproto::id(ack), ack);
}

void WorkerStreaming::onChangeStreamingParamsAck(
    const std::shared_ptr<google::protobuf::MessageLite>& _msg) {
    auto msg = std::static_pointer_cast<ltproto::client2worker::ChangeStreamingParamsAck>(_msg);
    // 无论如何都不应该再改用户主动改过的分辨率
    if (settings_) {
        settings_->deleteKey("old_screen_width");
        settings_->deleteKey("old_screen_height");
        settings_->deleteKey("old_screen_rate");
        settings_ = nullptr;
    }
    if (msg->err_code() != ltproto::ErrorCode::Success) {
        LOG(ERR) << "Received ChangeStreamingParamsAck with error code "
                 << static_cast<int32_t>(msg->err_code()) << " : "
                 << ltproto::ErrorCode_Name(msg->err_code());
        stop(kExitCodeClientChangeStreamingParamsFailed);
    }
    else {
        stop(kExitCodeRestartResolutionChanged);
    }
}

void WorkerStreaming::onSwitchMonitor(const std::shared_ptr<google::protobuf::MessageLite>&) {
    if (monitors_.size() <= 1) {
        return;
    }
    video_->stop();
    auto next_index = (monitor_index_ + 1) % monitors_.size();
    auto msg = std::make_shared<ltproto::client2worker::ChangeStreamingParams>();
    msg->mutable_params()->set_video_width(monitors_[next_index].width);
    msg->mutable_params()->set_video_height(monitors_[next_index].height);
    msg->mutable_params()->set_rotation(monitors_[next_index].rotation);
    msg->mutable_params()->set_monitor_index(static_cast<int32_t>(next_index));
    sendPipeMessage(ltproto::id(msg), msg);
}

} // namespace worker

} // namespace lt