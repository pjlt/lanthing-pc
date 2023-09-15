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

#include "client.h"

#include <g3log/g3log.hpp>
#include <ltproto/ltproto.h>

#include <ltproto/peer2peer/keep_alive.pb.h>
#include <ltproto/peer2peer/request_keyframe.pb.h>
#include <ltproto/peer2peer/start_transmission.pb.h>
#include <ltproto/peer2peer/start_transmission_ack.pb.h>
#include <ltproto/peer2peer/time_sync.pb.h>
#include <ltproto/server/request_connection.pb.h>
#include <ltproto/server/request_connection_ack.pb.h>
#include <ltproto/signaling/join_room.pb.h>
#include <ltproto/signaling/join_room_ack.pb.h>
#include <ltproto/signaling/signaling_message.pb.h>
#include <ltproto/signaling/signaling_message_ack.pb.h>

#include <ltlib/time_sync.h>
#include <string_keys.h>

#if LT_USE_LTRTC
#include <rtc/rtc.h>
#else // LT_USE_LTRTC
#include <transport/transport_tcp.h>
#endif // LT_USE_LTRTC

namespace {

lt::VideoCodecType to_ltrtc(std::string codec_str) {
    static const std::string kAVC = "avc";
    static const std::string kHEVC = "hevc";
    std::transform(codec_str.begin(), codec_str.end(), codec_str.begin(), std::tolower);
    if (codec_str == kAVC) {
        return lt::VideoCodecType::H264;
    }
    else if (codec_str == kHEVC) {
        return lt::VideoCodecType::H265;
    }
    else {
        return lt::VideoCodecType::Unknown;
    }
}

lt::AudioCodecType atype() {
#if LT_USE_LTRTC
    return lt::AudioCodecType::PCM;
#else
    return lt::AudioCodecType::OPUS;
#endif
}

} // namespace

namespace lt {

namespace cli {

std::unique_ptr<Client> Client::create(std::map<std::string, std::string> options) {
    if (options.find("-cid") == options.end() || options.find("-rid") == options.end() ||
        options.find("-token") == options.end() || options.find("-user") == options.end() ||
        options.find("-pwd") == options.end() || options.find("-addr") == options.end() ||
        options.find("-port") == options.end() || options.find("-codec") == options.end() ||
        options.find("-width") == options.end() || options.find("-height") == options.end() ||
        options.find("-freq") == options.end() || options.find("-dinput") == options.end() ||
        options.find("-gamepad") == options.end() || options.find("-chans") == options.end() ||
        options.find("-afreq") == options.end()) {
        LOG(WARNING) << "Parameter invalid";
        return nullptr;
    }
    Params params{};
    params.client_id = options["-cid"];
    params.room_id = options["-rid"];
    params.auth_token = options["-token"];
    params.signaling_addr = options["-addr"];
    params.user = options["-user"];
    params.pwd = options["-pwd"];
    int32_t signaling_port = std::atoi(options["-port"].c_str());
    params.codec = options["-codec"];
    int32_t width = std::atoi(options["-width"].c_str());
    int32_t height = std::atoi(options["-height"].c_str());
    int32_t freq = std::atoi(options["-freq"].c_str());
    int32_t audio_freq = std::atoi(options["-afreq"].c_str());
    int32_t audio_channels = std::atoi(options["-chans"].c_str());
    params.enable_driver_input = std::atoi(options["-dinput"].c_str()) != 0;
    params.enable_gamepad = std::atoi(options["-gamepad"].c_str()) != 0;

    if (options.find("-reflexs") != options.end()) {
        std::string reflexs_str = options["-reflexs"];
        std::stringstream ss{reflexs_str};
        std::string out;
        while (std::getline(ss, out, ',')) {
            params.reflex_servers.push_back(out);
        }
    }

    if (signaling_port <= 0 || signaling_port > 65535) {
        LOG(WARNING) << "Invalid parameter: port";
        return nullptr;
    }
    params.signaling_port = static_cast<uint16_t>(signaling_port);

    if (width <= 0) {
        LOG(WARNING) << "Invalid parameter: width";
        return nullptr;
    }
    params.width = static_cast<uint32_t>(width);

    if (height <= 0) {
        LOG(WARNING) << "Invalid parameter: height";
        return nullptr;
    }
    params.height = static_cast<uint32_t>(height);

    if (freq <= 0) {
        LOG(WARNING) << "Invalid parameter: freq";
        return nullptr;
    }
    params.screen_refresh_rate = static_cast<uint32_t>(freq);

    if (audio_channels <= 0) {
        LOG(WARNING) << "Invalid parameter: achans";
        return nullptr;
    }
    params.audio_channels = static_cast<uint32_t>(audio_channels);

    if (audio_freq <= 0) {
        LOG(WARNING) << "Invalid parameter: afreq";
        return nullptr;
    }
    params.audio_freq = static_cast<uint32_t>(audio_freq);

    std::unique_ptr<Client> client{new Client{params}};
    if (!client->init()) {
        return false;
    }
    return client;
}

Client::Client(const Params& params)
    : auth_token_{params.auth_token}
    , p2p_username_{params.user}
    , p2p_password_{params.pwd}
    , signaling_params_{params.client_id, params.room_id, params.signaling_addr,
                        params.signaling_port}
    , video_params_{to_ltrtc(params.codec), params.width, params.height, params.screen_refresh_rate,
                    std::bind(&Client::sendMessageToHost, this, std::placeholders::_1,
                              std::placeholders::_2, std::placeholders::_3)}
    , audio_params_{atype(), params.audio_freq, params.audio_channels}
    , reflex_servers_{params.reflex_servers} {}

Client::~Client() {
    {
        std::lock_guard lock{ioloop_mutex_};
        signaling_client_.reset();
        ioloop_.reset();
    }
}

bool Client::init() {
    ioloop_ = ltlib::IOLoop::create();
    if (ioloop_ == nullptr) {
        LOG(WARNING) << "Init IOLoop failed";
        return false;
    }
    ltlib::Client::Params params{};
    params.stype = ltlib::StreamType::TCP;
    params.ioloop = ioloop_.get();
    params.host = signaling_params_.addr;
    params.port = signaling_params_.port;
    params.is_tls = false;
    params.on_connected = std::bind(&Client::onSignalingConnected, this);
    params.on_closed = std::bind(&Client::onSignalingDisconnected, this);
    params.on_reconnecting = std::bind(&Client::onSignalingReconnecting, this);
    params.on_message = std::bind(&Client::onSignalingNetMessage, this, std::placeholders::_1,
                                  std::placeholders::_2);
    signaling_client_ = ltlib::Client::create(params);
    if (signaling_client_ == nullptr) {
        LOG(INFO) << "Create signaling client failed";
        return false;
    }
    hb_thread_ = ltlib::TaskThread::create("heart_beat");
    main_thread_ = ltlib::BlockingThread::create(
        "main_thread", [this](const std::function<void()>& i_am_alive) { mainLoop(i_am_alive); });
    should_exit_ = false;
    return true;
}

void Client::wait() {
    std::unique_lock<std::mutex> lock{exit_mutex_};
    exit_cv_.wait(lock, [this]() { return should_exit_; });
}

void Client::mainLoop(const std::function<void()>& i_am_alive) {
    LOG(INFO) << "Lanthing client enter main loop";
    ioloop_->run(i_am_alive);
    LOG(INFO) << "Lanthing client exit main loop";
}

void Client::onPlatformRenderTargetReset() {
    // NOTE: 这运行在platform线程
    std::lock_guard lock{dr_mutex_};
    video_pipeline_.reset();
    video_pipeline_ = VideoDecodeRenderPipeline::create(video_params_);
    if (video_pipeline_ == nullptr) {
        LOG(WARNING) << "Create VideoDecodeRenderPipeline failed";
    }
}

void Client::onPlatformExit() {
    postTask([this]() {
        auto msg = std::make_shared<ltproto::signaling::SignalingMessage>();
        msg->set_level(ltproto::signaling::SignalingMessage_Level_Core);
        auto coremsg = msg->mutable_core_message();
        coremsg->set_key(kSigCoreClose);
        signaling_client_->send(ltproto::id(msg), msg, [this]() { stopWait(); });
    });
}

void Client::stopWait() {
    {
        std::lock_guard<std::mutex> lock{exit_mutex_};
        should_exit_ = true;
    }
    exit_cv_.notify_one();
}

void Client::postTask(const std::function<void()>& task) {
    std::lock_guard lock{ioloop_mutex_};
    if (ioloop_) {
        ioloop_->post(task);
    }
}

void Client::postDelayTask(int64_t delay_ms, const std::function<void()>& task) {
    std::lock_guard lock{ioloop_mutex_};
    if (ioloop_) {
        ioloop_->postDelay(delay_ms, task);
    }
}

void Client::syncTime() {
    auto msg = std::make_shared<ltproto::peer2peer::TimeSync>();
    msg->set_t0(time_sync_.getT0());
    msg->set_t1(time_sync_.getT1());
    msg->set_t2(ltlib::steady_now_us());
    sendMessageToHost(ltproto::id(msg), msg, true);
    constexpr uint32_t k500ms = 500;
    postDelayTask(k500ms, std::bind(&Client::syncTime, this));
}

void Client::onSignalingNetMessage(uint32_t type,
                                   std::shared_ptr<google::protobuf::MessageLite> msg) {
    namespace ltype = ltproto::type;
    switch (type) {
    case ltype::kJoinRoomAck:
        onJoinRoomAck(msg);
        break;
    case ltype::kSignalingMessage:
        onSignalingMessage(msg);
        break;
    case ltype::kSignalingMessageAck:
        onSignalingMessageAck(msg);
        break;
    default:
        LOG(WARNING) << "Unknown signaling type";
        break;
    }
}

void Client::onSignalingDisconnected() {
    // TODO: 业务代码，目前是直接退出进程.
    stopWait();
}

void Client::onSignalingReconnecting() {
    LOG(INFO) << "Reconnecting signaling server...";
}

void Client::onSignalingConnected() {
    LOG(INFO) << "Connected to signaling server";
    auto msg = std::make_shared<ltproto::signaling::JoinRoom>();
    msg->set_session_id(signaling_params_.client_id);
    msg->set_room_id(signaling_params_.room_id);
    ioloop_->post([this, msg]() { signaling_client_->send(ltproto::id(msg), msg); });
}

void Client::onJoinRoomAck(std::shared_ptr<google::protobuf::MessageLite> _msg) {
    auto msg = std::static_pointer_cast<ltproto::signaling::JoinRoomAck>(_msg);
    if (msg->err_code() != ltproto::signaling::JoinRoomAck_ErrCode_Success) {
        LOG(INFO) << "Join room " << signaling_params_.room_id << " with id "
                  << signaling_params_.client_id << " failed";
        return;
    }
    LOG(INFO) << "Join signaling room success";
    PcSdl::Params params{};
    params.on_reset = std::bind(&Client::onPlatformRenderTargetReset, this);
    params.on_exit = std::bind(&Client::onPlatformExit, this);
    sdl_ = PcSdl::create(params);
    if (sdl_ == nullptr) {
        LOG(INFO) << "Initialize sdl failed";
        return;
    }
    LOG(INFO) << "Initialize SDL success";
    video_params_.sdl = sdl_.get();
    input_params_.sdl = sdl_.get();
    if (!initTransport()) {
        LOG(INFO) << "Initialize rtc failed";
        return;
    }
    LOG(INFO) << "Initialize rtc success";
}

void Client::onSignalingMessage(std::shared_ptr<google::protobuf::MessageLite> _msg) {
    auto msg = std::static_pointer_cast<ltproto::signaling::SignalingMessage>(_msg);
    switch (msg->level()) {
    case ltproto::signaling::SignalingMessage::Core:
        // 暂时没有需要在这层处理的信令消息
        break;
    case ltproto::signaling::SignalingMessage::Rtc:
    {
        auto& rtc_msg = msg->rtc_message();
        tp_client_->onSignalingMessage(rtc_msg.key().c_str(), rtc_msg.value().c_str());
        break;
    }
    default:
        break;
    }
}

void Client::onSignalingMessageAck(std::shared_ptr<google::protobuf::MessageLite> _msg) {
    auto msg = std::static_pointer_cast<ltproto::signaling::SignalingMessageAck>(_msg);
    switch (msg->err_code()) {
    case ltproto::signaling::SignalingMessageAck_ErrCode_Success:
        // do nothing
        break;
    case ltproto::signaling::SignalingMessageAck_ErrCode_NotOnline:
        LOG(INFO) << "Send signaling message failed, remote device not online";
        break;
    default:
        LOG(INFO) << "Send signaling message failed";
        break;
    }
}

bool Client::initTransport() {
    namespace ph = std::placeholders;
#if LT_USE_LTRTC
    rtc::Client::Params params{};
    params.use_nbp2p = true;
    std::vector<const char*> reflex_servers;
    for (auto& svr : reflex_servers_) {
        reflex_servers.push_back(svr.data());
        // LOG(DEBUG) << "Reflex: " << svr;
    }
    if (params.use_nbp2p) {
        params.nbp2p_params.disable_ipv6 = false;
        params.nbp2p_params.disable_lan_udp = false;
        params.nbp2p_params.disable_mapping = false;
        params.nbp2p_params.disable_reflex = false;
        params.nbp2p_params.disable_relay = false;
        params.nbp2p_params.username = p2p_username_.c_str();
        params.nbp2p_params.password = p2p_password_.c_str();
        params.nbp2p_params.reflex_servers = reflex_servers.data();
        params.nbp2p_params.reflex_servers_count = static_cast<uint32_t>(reflex_servers.size());
        params.nbp2p_params.relay_servers = nullptr;
        params.nbp2p_params.relay_servers_count = 0;
    }
    params.on_data = std::bind(&Client::onTpData, this, ph::_1, ph::_2, ph::_3);
    params.on_video = std::bind(&Client::onTpVideoFrame, this, ph::_1);
    params.on_audio = std::bind(&Client::onTpAudioData, this, ph::_1);
    params.on_connected = std::bind(&Client::onTpConnected, this);
    params.on_conn_changed = std::bind(&Client::onTpConnChanged, this);
    params.on_failed = std::bind(&Client::onTpFailed, this);
    params.on_disconnected = std::bind(&Client::onTpDisconnected, this);
    params.on_signaling_message = std::bind(&Client::onTpSignalingMessage, this, ph::_1, ph::_2);
    params.video_codec_type = video_params_.codec_type;
    params.audio_channels = audio_params_.channels;
    params.audio_sample_rate = audio_params_.frames_per_second;
    tp_client_ = rtc::Client::create(std::move(params));
#else  // LT_USE_LTRTC
    lt::tp::ClientTCP::Params params{};
    params.on_data = std::bind(&Client::onTpData, this, ph::_1, ph::_2, ph::_3);
    params.on_video = std::bind(&Client::onTpVideoFrame, this, ph::_1);
    params.on_audio = std::bind(&Client::onTpAudioData, this, ph::_1);
    params.on_connected = std::bind(&Client::onTpConnected, this);
    params.on_failed = std::bind(&Client::onTpFailed, this);
    params.on_disconnected = std::bind(&Client::onTpDisconnected, this);
    params.onSignalingMessage = std::bind(&Client::onTpSignalingMessage, this, ph::_1, ph::_2);
    params.video_codec_type = video_params_.codec_type;
    tp_client_ = lt::tp::ClientTCP::create(params);
#endif // LT_USE_LTRTC

    if (tp_client_ == nullptr) {
        LOG(INFO) << "Create rtc client failed";
        return false;
    }
    if (!tp_client_->connect()) {
        LOG(INFO) << "LTClient connect failed";
        return false;
    }
    return true;
}

void Client::onTpData(const uint8_t* data, uint32_t size, bool is_reliable) {
    (void)is_reliable;
    auto type = reinterpret_cast<const uint32_t*>(data);
    auto msg = ltproto::create_by_type(*type);
    if (msg == nullptr) {
        LOG(INFO) << "Unknown message type: " << *type;
    }
    bool success = msg->ParseFromArray(data + 4, size - 4);
    if (!success) {
        LOG(INFO) << "Parse message failed, type: " << *type;
        return;
    }
    dispatchRemoteMessage(*type, msg);
}

void Client::onTpVideoFrame(const lt::VideoFrame& frame) {
    std::lock_guard lock{dr_mutex_};
    if (video_pipeline_ == nullptr) {
        return;
    }
    VideoDecodeRenderPipeline::Action action = video_pipeline_->submit(frame);
    switch (action) {
    case VideoDecodeRenderPipeline::Action::REQUEST_KEY_FRAME:
    {
        auto req = std::make_shared<ltproto::peer2peer::RequestKeyframe>();
        sendMessageToHost(ltproto::id(req), req, true);
        break;
    }
    case VideoDecodeRenderPipeline::Action::NONE:
        break;
    default:
        break;
    }
}

void Client::onTpAudioData(const lt::AudioData& audio_data) {
    // FIXME: transport在audio_player_实例化前，不应回调audio数据
    if (audio_player_) {
        audio_player_->submit(audio_data.data, audio_data.size);
    }
}

void Client::onTpConnected() {
    video_pipeline_ = VideoDecodeRenderPipeline::create(video_params_);
    if (video_pipeline_ == nullptr) {
        LOG(WARNING) << "Create VideoDecodeRenderPipeline failed";
        return;
    }
    input_params_.send_message = std::bind(&Client::sendMessageToHost, this, std::placeholders::_1,
                                           std::placeholders::_2, std::placeholders::_3);
    input_params_.host_height = video_params_.height;
    input_params_.host_width = video_params_.width;
    input_capturer_ = InputCapturer::create(input_params_);
    if (input_capturer_ == nullptr) {
        LOG(WARNING) << "Create InputCapturer failed";
        return;
    }
    audio_player_ = AudioPlayer::create(audio_params_);
    if (audio_player_ == nullptr) {
        LOG(INFO) << "Create AudioPlayer failed";
        return;
    }
    hb_thread_->post(std::bind(&Client::sendKeepAlive, this));
    // 如果未来有“串流”以外的业务，在这个StartTransmission添加字段.
    auto start = std::make_shared<ltproto::peer2peer::StartTransmission>();
    start->set_client_os(ltproto::peer2peer::StartTransmission_ClientOS_Windows);
    start->set_token(auth_token_);
    sendMessageToHost(ltproto::id(start), start, true);
    postTask(std::bind(&Client::syncTime, this));
}

void Client::onTpConnChanged() {}

void Client::onTpFailed() {
    stopWait();
}

void Client::onTpDisconnected() {
    stopWait();
}

void Client::onTpSignalingMessage(const std::string& key, const std::string& value) {
    // 将key和value封装在proto里.
    auto msg = std::make_shared<ltproto::signaling::SignalingMessage>();
    msg->set_level(ltproto::signaling::SignalingMessage::Rtc);
    auto rtc_msg = msg->mutable_rtc_message();
    rtc_msg->set_key(key);
    rtc_msg->set_value(value);
    postTask([this, msg]() { signaling_client_->send(ltproto::id(msg), msg); });
}

void Client::dispatchRemoteMessage(uint32_t type,
                                   const std::shared_ptr<google::protobuf::MessageLite>& msg) {
    switch (type) {
    case ltproto::type::kStartTransmissionAck:
        onStartTransmissionAck(msg);
        break;
    case ltproto::type::kTimeSync:
        onTimeSync(msg);
        break;
    default:
        LOG(WARNING) << "Unknown message type: " << type;
        break;
    }
}

void Client::sendKeepAlive() {
    auto keep_alive = std::make_shared<ltproto::peer2peer::KeepAlive>();
    sendMessageToHost(ltproto::id(keep_alive), keep_alive, true);

    const auto k500ms = ltlib::TimeDelta{500'000};
    hb_thread_->post_delay(k500ms, std::bind(&Client::sendKeepAlive, this));
}

bool Client::sendMessageToHost(uint32_t type,
                               const std::shared_ptr<google::protobuf::MessageLite>& msg,
                               bool reliable) {
    auto packet = ltproto::Packet::create({type, msg}, false);
    if (!packet.has_value()) {
        LOG(WARNING) << "Create Peer2Peer packet failed, type:" << type;
        return false;
    }
    const auto& pkt = packet.value();
    // WebRTC的数据通道可以帮助我们完成stream->packet的过程，所以这里不需要把packet
    // header一起传过去.
    bool success = tp_client_->sendData(pkt.payload.get(), pkt.header.payload_size, reliable);
    return success;
}

void Client::onStartTransmissionAck(const std::shared_ptr<google::protobuf::MessageLite>& _msg) {
    auto msg = std::static_pointer_cast<ltproto::peer2peer::StartTransmissionAck>(_msg);
    if (msg->err_code() == ltproto::peer2peer::StartTransmissionAck_ErrCode_Success) {
        LOG(INFO) << "Received StartTransmissionAck with success";
    }
    else {
        LOG(INFO) << "StartTransmission failed with "
                  << ltproto::peer2peer::StartTransmissionAck_ErrCode_Name(msg->err_code());
        stopWait();
    }
}

void Client::onTimeSync(std::shared_ptr<google::protobuf::MessageLite> _msg) {
    auto msg = std::static_pointer_cast<ltproto::peer2peer::TimeSync>(_msg);
    auto result = time_sync_.calc(msg->t0(), msg->t1(), msg->t2(), ltlib::steady_now_us());
    if (result.has_value()) {
        rtt_ = result->rtt;
        time_diff_ = result->time_diff;
        LOG(DEBUG) << "rtt:" << rtt_ << ", time_diff:" << time_diff_;
        if (video_pipeline_) {
            video_pipeline_->setTimeDiff(time_diff_);
        }
    }
}

} // namespace cli

} // namespace lt