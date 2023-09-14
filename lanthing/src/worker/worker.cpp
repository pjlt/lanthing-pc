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

#include "worker.h"

#include <g3log/g3log.hpp>

#include <ltlib/times.h>
#include <ltproto/ltproto.h>

#include <ltproto/peer2peer/audio_data.pb.h>
#include <ltproto/peer2peer/capture_video_frame_ack.pb.h>
#include <ltproto/peer2peer/start_working.pb.h>
#include <ltproto/peer2peer/start_working_ack.pb.h>
#include <ltproto/peer2peer/streaming_params.pb.h>

namespace {

lt::VideoCodecType to_ltrtc(ltproto::peer2peer::VideoCodecType codec_type) {
    switch (codec_type) {
    case ltproto::peer2peer::AVC:
        return lt::VideoCodecType::H264;
    case ltproto::peer2peer::HEVC:
        return lt::VideoCodecType::H265;
    default:
        return lt::VideoCodecType::Unknown;
    }
}

ltproto::peer2peer::StreamingParams::VideoEncodeBackend
to_protobuf(lt::VideoEncoder::Backend backend) {
    switch (backend) {
    case lt::VideoEncoder::Backend::NvEnc:
        return ltproto::peer2peer::StreamingParams_VideoEncodeBackend_NvEnc;
    case lt::VideoEncoder::Backend::IntelMediaSDK:
        return ltproto::peer2peer::StreamingParams_VideoEncodeBackend_IntelMediaSDK;
    case lt::VideoEncoder::Backend::Amf:
        return ltproto::peer2peer::StreamingParams_VideoEncodeBackend_AMF;
    default:
        return ltproto::peer2peer::StreamingParams_VideoEncodeBackend_UnknownVideoEncode;
    }
}

ltproto::peer2peer::VideoCodecType to_protobuf(lt::VideoCodecType codec_type) {
    switch (codec_type) {
    case lt::VideoCodecType::H264:
        return ltproto::peer2peer::VideoCodecType::AVC;
    case lt::VideoCodecType::H265:
        return ltproto::peer2peer::VideoCodecType::HEVC;
    default:
        return ltproto::peer2peer::VideoCodecType::UnknownVCT;
    }
}

std::string to_string(lt::VideoCodecType type) {
    switch (type) {
    case lt::VideoCodecType::H264:
        return "AVC";
    case lt::VideoCodecType::H265:
        return "HEVC";
    default:
        return "Unknown Codec";
    }
}

} // namespace

namespace lt {

namespace worker {

std::unique_ptr<Worker> Worker::create(std::map<std::string, std::string> options) {
    if (options.find("-width") == options.end() || options.find("-height") == options.end() ||
        options.find("-freq") == options.end() || options.find("-codecs") == options.end() ||
        options.find("-name") == options.end()) {
        LOG(WARNING) << "Parameter invalid";
        return nullptr;
    }
    Params params{};
    int32_t width = std::atoi(options["-width"].c_str());
    int32_t height = std::atoi(options["-height"].c_str());
    int32_t freq = std::atoi(options["-freq"].c_str());
    std::stringstream ss(options["-codecs"]);

    params.name = options["-name"];
    if (params.name.empty()) {
        LOG(WARNING) << "Parameter invalid: name";
        return nullptr;
    }
    if (width <= 0) {
        LOG(WARNING) << "Parameter invalid: width";
        return nullptr;
    }
    params.width = static_cast<uint32_t>(width);
    if (height <= 0) {
        LOG(WARNING) << "Parameter invalid: height";
        return nullptr;
    }
    params.height = static_cast<uint32_t>(height);
    if (freq <= 0) {
        LOG(WARNING) << "Parameter invalid: freq";
        return nullptr;
    }
    params.refresh_rate = static_cast<uint32_t>(freq);
    std::string codec;
    while (std::getline(ss, codec, ',')) {
        if (codec == "avc") {
            params.codecs.push_back(lt::VideoCodecType::H264);
        }
        else if (codec == "hevc") {
            params.codecs.push_back(lt::VideoCodecType::H265);
        }
    }
    if (params.codecs.empty()) {
        LOG(WARNING) << "Parameter invalid: codecs";
        return nullptr;
    }

    std::unique_ptr<Worker> worker{new Worker{params}};
    if (!worker->init()) {
        return nullptr;
    }
    return worker;
}

Worker::Worker(const Params& params)
    : client_width_{params.width}
    , client_height_{params.height}
    , client_refresh_rate_{params.refresh_rate}
    , client_codec_types_{params.codecs}
    , pipe_name_{params.name}
    , last_time_received_from_service_{ltlib::steady_now_ms()} {}

Worker::~Worker() {
    {
        std::lock_guard lock{mutex_};
        pipe_client_.reset();
        ioloop_.reset();
    }
}

void Worker::wait() {
    session_observer_->waitForChange();
}

bool Worker::init() {
    session_observer_ = SessionChangeObserver::create();
    if (session_observer_ == nullptr) {
        LOG(WARNING) << "Create session observer failed";
        return false;
    }
    ioloop_ = ltlib::IOLoop::create();
    if (ioloop_ == nullptr) {
        LOG(WARNING) << "Create IOLoop failed";
        return false;
    }
    if (!initPipeClient()) {
        LOG(WARNING) << "Init pipe client failed";
        return false;
    }
    DisplaySetting client_display_setting{client_width_, client_height_, client_refresh_rate_};
    negotiated_display_setting_ = DisplaySettingNegotiator::negotiate(client_display_setting);
    if (negotiated_display_setting_.width == 0 || negotiated_display_setting_.height == 0) {
        LOG(WARNING) << "Negotiate display setting failed, fallback to default(width:1920, "
                        "height:1080, refresh_rate:60)";
        negotiated_display_setting_.width = 1920;
        negotiated_display_setting_.height = 1080;
    }
    else {
        LOGF(DEBUG, "Negotiate display setting(width:%d, height:%d, refresh_rate:%d)",
             negotiated_display_setting_.width, negotiated_display_setting_.height,
             negotiated_display_setting_.refrash_rate);
    }
    if (!negotiateParameters()) {
        return false;
    }

    namespace ltype = ltproto::type;
    namespace ph = std::placeholders;
    const std::pair<uint32_t, MessageHandler> handlers[] = {
        {ltype::kStartWorking, std::bind(&Worker::onStartWorking, this, ph::_1)},
        {ltype::kStopWorking, std::bind(&Worker::onStopWorking, this, ph::_1)},
        {ltype::kKeepAlive, std::bind(&Worker::onKeepAlive, this, ph::_1)},
        {ltype::kCaptureVideoFrameAck, std::bind(&Worker::onFrameAck, this, ph::_1)}};
    for (auto& handler : handlers) {
        if (!registerMessageHandler(handler.first, handler.second)) {
            LOG(FATAL) << "Register message handler(" << handler.first << ") failed";
        }
    }

    ioloop_->postDelay(500 /*ms*/, std::bind(&Worker::checkCimeout, this));
    std::promise<void> promise;
    auto future = promise.get_future();
    thread_ = ltlib::BlockingThread::create(
        "main_thread",
        [this, &promise](const std::function<void()>& i_am_alive, void*) {
            promise.set_value();
            mainLoop(i_am_alive);
        },
        nullptr);
    future.get();
    return true;
} // namespace lt

bool Worker::initPipeClient() {
    ltlib::Client::Params params{};
    params.stype = ltlib::StreamType::Pipe;
    params.ioloop = ioloop_.get();
    params.pipe_name = "\\\\?\\pipe\\" + pipe_name_;
    params.is_tls = false;
    params.on_closed = std::bind(&Worker::onPipeDisconnected, this);
    params.on_connected = std::bind(&Worker::onPipeConnected, this);
    params.on_message =
        std::bind(&Worker::onPipeMessage, this, std::placeholders::_1, std::placeholders::_2);
    params.on_reconnecting = std::bind(&Worker::onPipeReconnecting, this);
    pipe_client_ = ltlib::Client::create(params);
    return pipe_client_ != nullptr;
}

bool Worker::negotiateParameters() {
    auto negotiated_params = std::make_shared<ltproto::peer2peer::StreamingParams>();

    lt::AudioCapturer::Params audio_params{};
#if LT_USE_LTRTC
    audio_params.type = AudioCodecType::PCM;
#else
    audio_params.type = AudioCodecType::OPUS;
#endif
    audio_params.on_audio_data =
        std::bind(&Worker::onCapturedAudioData, this, std::placeholders::_1);
    auto audio_capturer = AudioCapturer::create(audio_params);
    if (audio_capturer == nullptr) {
        return false;
    }
    negotiated_params->set_audio_channels(audio_capturer->channels());
    negotiated_params->set_audio_sample_rate(audio_capturer->framesPerSec());

    lt::VideoCapturer::Params video_params{};
    video_params.backend = lt::VideoCapturer::Backend::Dxgi;
    video_params.on_frame = std::bind(&Worker::onCapturedVideoFrame, this, std::placeholders::_1);
    auto video_capturer = lt::VideoCapturer::create(video_params);
    if (video_capturer == nullptr) {
        LOGF(WARNING, "Create VideoCapturer with(backend:%d) failed", video_params.backend);
        return false;
    }
    std::vector<VideoEncoder::Ability> encode_abilities;
    if (video_params.backend == VideoCapturer::Backend::Dxgi) {
        negotiated_params->set_video_capture_backend(
            ltproto::peer2peer::StreamingParams_VideoCaptureBackend_Dxgi);
        negotiated_params->set_luid(video_capturer->luid());
        encode_abilities = VideoEncoder::checkEncodeAbilitiesWithLuid(
            video_capturer->luid(), negotiated_display_setting_.width,
            negotiated_display_setting_.height);
    }
    else {
        negotiated_params->set_video_capture_backend(
            ltproto::peer2peer::StreamingParams_VideoCaptureBackend_UnknownVideoCapture);
        encode_abilities = VideoEncoder::checkEncodeAbilities(client_width_, client_height_);
    }
    negotiated_params->set_enable_driver_input(false);
    negotiated_params->set_enable_gamepad(false);
    negotiated_params->set_screen_refresh_rate(negotiated_display_setting_.refrash_rate);
    negotiated_params->set_video_width(negotiated_display_setting_.width);
    negotiated_params->set_video_height(negotiated_display_setting_.height);
    auto negotiated_video_codecs = negotiated_params->mutable_video_codecs();
    for (const auto& encode_ability : encode_abilities) {
        if (!negotiated_video_codecs->empty()) {
            break;
        }
        for (const auto& client_codec_type : client_codec_types_) {
            if (client_codec_type == encode_ability.codec_type) {
                ltproto::peer2peer::StreamingParams::VideoCodec video_codec;
                video_codec.set_backend(::to_protobuf(encode_ability.backend));
                video_codec.set_codec_type(::to_protobuf(encode_ability.codec_type));
                negotiated_video_codecs->Add(std::move(video_codec));
                negotiated_video_codec_beckend_ = encode_ability.backend;
                negotiated_video_codec_type_ = encode_ability.codec_type;
                LOG(INFO) << "Negotiated video codec:" << to_string(encode_ability.codec_type);
                break;
            }
        }
    }
    if (negotiated_video_codecs->empty()) {
        std::stringstream ss;
        ss << "Negotiate video codec failed, client supports codec:[";
        for (auto codec : client_codec_types_) {
            ss << to_string(codec) << " ";
        }
        ss << "], host supports codec:[";
        for (auto& codec : encode_abilities) {
            ss << to_string(codec.codec_type) << " ";
        }
        ss << "]";
        LOG(WARNING) << ss.str();
    }
    negotiated_params_ = negotiated_params;
    video_capturer_ = std::move(video_capturer);
    audio_capturer_ = std::move(audio_capturer);
    return true;
}

void Worker::mainLoop(const std::function<void()>& i_am_alive) {
    LOG(INFO) << "Worker enter main loop";
    ioloop_->run(i_am_alive);
    LOG(INFO) << "Worker exit main loop";
}

void Worker::stop() {
    session_observer_->stop();
}

void Worker::postTask(const std::function<void()>& task) {
    std::lock_guard lock{mutex_};
    if (ioloop_) {
        ioloop_->post(task);
    }
}

void Worker::postDelayTask(int64_t delay_ms, const std::function<void()>& task) {
    std::lock_guard lock{mutex_};
    if (ioloop_) {
        ioloop_->postDelay(delay_ms, task);
    }
}

bool Worker::registerMessageHandler(uint32_t type, const MessageHandler& handler) {
    auto [_, success] = msg_handlers_.insert({type, handler});
    if (!success) {
        LOG(WARNING) << "Register message handler(" << type << ") failed";
        return false;
    }
    else {
        return true;
    }
}

void Worker::dispatchServiceMessage(uint32_t type,
                                    const std::shared_ptr<google::protobuf::MessageLite>& msg) {
    auto iter = msg_handlers_.find(type);
    if (iter == msg_handlers_.cend() || iter->second == nullptr) {
        LOG(WARNING) << "Unknown message type: " << type;
        return;
    }
    iter->second(msg);
}

bool Worker::sendPipeMessage(uint32_t type,
                             const std::shared_ptr<google::protobuf::MessageLite>& msg) {
    if (!connected_to_service_) {
        return false;
    }
    return pipe_client_->send(type, msg);
}

void Worker::printStats() {}

void Worker::checkCimeout() {
    constexpr int64_t kTimeout = 3'000;
    auto now = ltlib::steady_now_ms();
    if (now - last_time_received_from_service_ > kTimeout) {
        LOG(WARNING) << "No packet from service for " << now - last_time_received_from_service_
                     << "ms, worker exit.";
        stop();
    }
    else {
        postDelayTask(500, [this]() { checkCimeout(); });
    }
}

void Worker::onCapturedVideoFrame(std::shared_ptr<google::protobuf::MessageLite> frame) {
    postTask([this, frame]() { sendPipeMessage(ltproto::type::kCaptureVideoFrame, frame); });
}

void Worker::onCapturedAudioData(std::shared_ptr<google::protobuf::MessageLite> audio_data) {
    postTask([this, audio_data]() { sendPipeMessage(ltproto::type::kAudioData, audio_data); });
}

void Worker::onPipeMessage(uint32_t type, std::shared_ptr<google::protobuf::MessageLite> msg) {
    dispatchServiceMessage(type, msg);
}

void Worker::onPipeDisconnected() {
    LOG(WARNING) << "Disconnected from service, won't reconnect again";
    connected_to_service_ = false;
}

void Worker::onPipeReconnecting() {
    LOG(INFO) << "Reconnecting to service...";
    connected_to_service_ = false;
}

void Worker::onPipeConnected() {
    if (connected_to_service_) {
        LOG(FATAL) << "Logic error, connected to service twice";
    }
    else {
        LOG(INFO) << "Connected to service";
    }
    connected_to_service_ = true;
    // 连上第一时间，向service发送协商好的串流参数
    auto params = std::static_pointer_cast<ltproto::peer2peer::StreamingParams>(negotiated_params_);
    sendPipeMessage(ltproto::id(params), params);
}

void Worker::onStartWorking(const std::shared_ptr<google::protobuf::MessageLite>& _msg) {
    auto msg = std::static_pointer_cast<ltproto::peer2peer::StartWorking>(_msg);
    auto ack = std::make_shared<ltproto::peer2peer::StartWorkingAck>();
    do {
        video_capturer_->start();
        audio_capturer_->start();

        InputExecutor::Params input_params{};
        input_params.types = static_cast<uint8_t>(InputExecutor::Type::WIN32_MESSAGE) |
                             static_cast<uint8_t>(InputExecutor::Type::WIN32_DRIVER);
        input_params.screen_width = negotiated_display_setting_.width;
        input_params.screen_height = negotiated_display_setting_.height;
        input_params.register_message_handler = std::bind(
            &Worker::registerMessageHandler, this, std::placeholders::_1, std::placeholders::_2);
        input_params.send_message =
            std::bind(&Worker::sendPipeMessage, this, std::placeholders::_1, std::placeholders::_2);
        input_ = InputExecutor::create(input_params);
        if (input_ == nullptr) {
            ack->set_err_code(ltproto::peer2peer::StartWorkingAck_ErrCode_InputFailed);
            break;
        }
        ack->set_err_code(ltproto::peer2peer::StartWorkingAck_ErrCode_Success);
    } while (false);
    for (const auto& handler : msg_handlers_) {
        ack->add_msg_type(handler.first);
    }

    if (ack->err_code() != ltproto::peer2peer::StartWorkingAck_ErrCode_Success) {
        if (video_capturer_) {
            video_capturer_->stop();
        }
        if (audio_capturer_) {
            audio_capturer_->stop();
        }
        input_ = nullptr;
    }
    sendPipeMessage(ltproto::id(ack), ack);
}

void Worker::onStopWorking(const std::shared_ptr<google::protobuf::MessageLite>&) {
    LOG(INFO) << "Received StopWorking";
    stop();
}

void Worker::onKeepAlive(const std::shared_ptr<google::protobuf::MessageLite>&) {
    last_time_received_from_service_ = ltlib::steady_now_ms();
}

void Worker::onFrameAck(const std::shared_ptr<google::protobuf::MessageLite>& msg) {

    auto ack = std::static_pointer_cast<ltproto::peer2peer::CaptureVideoFrameAck>(msg);
    LOG(DEBUG) << "Frame ack " << ack->name();
    video_capturer_->releaseFrame(ack->name());
}

} // namespace worker

} // namespace lt