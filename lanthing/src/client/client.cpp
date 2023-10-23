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

#include <sstream>

#include <ltproto/client2service/time_sync.pb.h>
#include <ltproto/client2worker/request_keyframe.pb.h>
#include <ltproto/client2worker/send_side_stat.pb.h>
#include <ltproto/client2worker/start_transmission.pb.h>
#include <ltproto/client2worker/start_transmission_ack.pb.h>
#include <ltproto/common/keep_alive.pb.h>
#include <ltproto/ltproto.h>
#include <ltproto/server/request_connection.pb.h>
#include <ltproto/server/request_connection_ack.pb.h>
#include <ltproto/signaling/join_room.pb.h>
#include <ltproto/signaling/join_room_ack.pb.h>
#include <ltproto/signaling/signaling_message.pb.h>
#include <ltproto/signaling/signaling_message_ack.pb.h>

#include <ltlib/logging.h>
#include <ltlib/time_sync.h>
#include <string_keys.h>

#include <transport/transport_rtc.h>
#include <transport/transport_rtc2.h>
#include <transport/transport_tcp.h>

namespace {

lt::VideoCodecType to_ltrtc(std::string codec_str) {
    static const std::string kAVC = "avc";
    static const std::string kHEVC = "hevc";
    std::transform(codec_str.begin(), codec_str.end(), codec_str.begin(),
                   [](char c) -> char { return (char)std::tolower(c); });
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
    switch (LT_TRANSPORT_TYPE) {
    case LT_TRANSPORT_RTC:
        return lt::AudioCodecType::PCM;
    case LT_TRANSPORT_RTC2:
        return lt::AudioCodecType::PCM;
    case LT_TRANSPORT_TCP:
        return lt::AudioCodecType::OPUS;
    default:
        LOG(FATAL) << "Unknown transport type";
        return lt::AudioCodecType::OPUS;
    }
}

std::string toString(lt::VideoCodecType codec) {
    switch (codec) {

    case lt::VideoCodecType::H264:
        return "AVC";
    case lt::VideoCodecType::H265:
        return "HEVC";
    case lt::VideoCodecType::Unknown:
    default:
        return "?";
    }
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
        LOG(ERR) << "Parameter invalid";
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

    LOG(INFO) << "Signaling " << params.signaling_addr.c_str() << ":" << signaling_port;

    if (options.find("-reflexs") != options.end()) {
        std::string reflexs_str = options["-reflexs"];
        std::stringstream ss{reflexs_str};
        std::string out;
        while (std::getline(ss, out, ',')) {
            params.reflex_servers.push_back(out);
        }
    }

    if (signaling_port <= 0 || signaling_port > 65535) {
        LOG(ERR) << "Invalid parameter: port";
        return nullptr;
    }
    params.signaling_port = static_cast<uint16_t>(signaling_port);

    if (width <= 0) {
        LOG(ERR) << "Invalid parameter: width";
        return nullptr;
    }
    params.width = static_cast<uint32_t>(width);

    if (height <= 0) {
        LOG(ERR) << "Invalid parameter: height";
        return nullptr;
    }
    params.height = static_cast<uint32_t>(height);

    if (freq <= 0) {
        LOG(ERR) << "Invalid parameter: freq";
        return nullptr;
    }
    params.screen_refresh_rate = static_cast<uint32_t>(freq);

    if (audio_channels <= 0) {
        LOG(ERR) << "Invalid parameter: achans";
        return nullptr;
    }
    params.audio_channels = static_cast<uint32_t>(audio_channels);

    if (audio_freq <= 0) {
        LOG(ERR) << "Invalid parameter: afreq";
        return nullptr;
    }
    params.audio_freq = static_cast<uint32_t>(audio_freq);

    std::unique_ptr<Client> client{new Client{params}};
    if (!client->init()) {
        return nullptr;
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
    if (tp_client_ != nullptr) {
        switch (LT_TRANSPORT_TYPE) {
        case LT_TRANSPORT_TCP:
        {
            auto tcp_cli = static_cast<lt::tp::ClientTCP*>(tp_client_);
            delete tcp_cli;
            break;
        }
        case LT_TRANSPORT_RTC:
        { // rtc.dll build on another machine!
            rtc::Client::destroy(tp_client_);
            break;
        }
        case LT_TRANSPORT_RTC2:
        {
            auto rtc2_cli = static_cast<rtc2::Client*>(tp_client_);
            delete rtc2_cli;
            break;
        }
        default:
            break;
        }
    }
}

bool Client::init() {
    if (!initSettings()) {
        LOG(ERR) << "Init settings failed";
        return false;
    }
    auto wf = settings_->getBoolean("windowed_fullscreen");
    if (!wf.has_value() || wf.value()) {
        // 没有设置 或者 设置为真，即默认窗口化全屏
        windowed_fullscreen_ = true;
    }
    ioloop_ = ltlib::IOLoop::create();
    if (ioloop_ == nullptr) {
        LOG(ERR) << "Init IOLoop failed";
        return false;
    }
    if (!initSignalingClient()) {
        LOG(ERR) << "Create signaling client failed";
        return false;
    }
    hb_thread_ = ltlib::TaskThread::create("heart_beat");
    main_thread_ = ltlib::BlockingThread::create(
        "main_thread", [this](const std::function<void()>& i_am_alive) { mainLoop(i_am_alive); });
    should_exit_ = false;
    return true;
}

bool Client::initSettings() {
    settings_ = ltlib::Settings::create(ltlib::Settings::Storage::Sqlite);
    return settings_ != nullptr;
}

#define MACRO_TO_STRING_HELPER(str) #str
#define MACRO_TO_STRING(str) MACRO_TO_STRING_HELPER(str)
#include <ISRG-Root.cert>
bool Client::initSignalingClient() {
    ltlib::Client::Params params{};
    params.stype = ltlib::StreamType::TCP;
    params.ioloop = ioloop_.get();
    params.host = signaling_params_.addr;
    params.port = signaling_params_.port;
    params.is_tls = LT_SERVER_USE_SSL;
    params.cert = kLanthingCert;
    params.on_connected = std::bind(&Client::onSignalingConnected, this);
    params.on_closed = std::bind(&Client::onSignalingDisconnected, this);
    params.on_reconnecting = std::bind(&Client::onSignalingReconnecting, this);
    params.on_message = std::bind(&Client::onSignalingNetMessage, this, std::placeholders::_1,
                                  std::placeholders::_2);
    signaling_client_ = ltlib::Client::create(params);
    return signaling_client_ != nullptr;
}
#undef MACRO_TO_STRING
#undef MACRO_TO_STRING_HELPER

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
    // video_pipeline_.reset();
    // video_pipeline_ = VideoDecodeRenderPipeline::create(video_params_);
    // if (video_pipeline_ == nullptr) {
    //     LOG(ERR) << "Create VideoDecodeRenderPipeline failed";
    // }
    video_pipeline_->resetRenderTarget();
}

void Client::onPlatformExit() {
    postTask([this]() {
        auto msg = std::make_shared<ltproto::signaling::SignalingMessage>();
        msg->set_level(ltproto::signaling::SignalingMessage_Level_Core);
        auto coremsg = msg->mutable_core_message();
        coremsg->set_key(kSigCoreClose);
        signaling_client_->send(ltproto::id(msg), msg, [this]() { stopWait(); });
    });
    // 保险起见
    postDelayTask(50, [this]() { stopWait(); });
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
    auto msg = std::make_shared<ltproto::client2service::TimeSync>();
    msg->set_t0(time_sync_.getT0());
    msg->set_t1(time_sync_.getT1());
    msg->set_t2(ltlib::steady_now_us());
    sendMessageToHost(ltproto::id(msg), msg, true);
    constexpr uint32_t k500ms = 500;
    postDelayTask(k500ms, std::bind(&Client::syncTime, this));
}

void Client::toggleFullscreen() {
    sdl_->toggleFullscreen();
}

void Client::onSignalingNetMessage(uint32_t type,
                                   std::shared_ptr<google::protobuf::MessageLite> msg) {
    namespace ltype = ltproto::type;
    switch (type) {
    case ltype::kKeepAliveAck:
        // do nothing
        break;
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
    if (!signaling_keepalive_inited_) {
        signaling_keepalive_inited_ = true;
        sendKeepaliveToSignalingServer();
    }
}

void Client::onJoinRoomAck(std::shared_ptr<google::protobuf::MessageLite> _msg) {
    auto msg = std::static_pointer_cast<ltproto::signaling::JoinRoomAck>(_msg);
    if (msg->err_code() != ltproto::ErrorCode::Success) {
        LOG(INFO) << "Join room " << signaling_params_.room_id << " with id "
                  << signaling_params_.client_id << " failed";
        return;
    }
    LOG(INFO) << "Join signaling room success";
    PcSdl::Params params{};
    params.on_reset = std::bind(&Client::onPlatformRenderTargetReset, this);
    params.on_exit = std::bind(&Client::onPlatformExit, this);
    params.windowed_fullscreen = windowed_fullscreen_;
    sdl_ = PcSdl::create(params);
    if (sdl_ == nullptr) {
        LOG(INFO) << "Initialize sdl failed";
        return;
    }
    LOG(INFO) << "Initialize SDL success";
    sdl_->setTitle("Connecting....");
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
        dispatchSignalingMessageCore(msg);
        break;
    case ltproto::signaling::SignalingMessage::Rtc:
        dispatchSignalingMessageRtc(msg);
        break;
    default:
        break;
    }
}

void Client::onSignalingMessageAck(std::shared_ptr<google::protobuf::MessageLite> _msg) {
    auto msg = std::static_pointer_cast<ltproto::signaling::SignalingMessageAck>(_msg);
    switch (msg->err_code()) {
    case ltproto::ErrorCode::Success:
        // do nothing
        break;
    case ltproto::ErrorCode::SignalingPeerNotOnline:
        LOG(INFO) << "Send signaling message failed, remote device not online";
        break;
    default:
        LOG(INFO) << "Send signaling message failed";
        break;
    }
}

void Client::dispatchSignalingMessageRtc(std::shared_ptr<google::protobuf::MessageLite> _msg) {
    auto msg = std::static_pointer_cast<ltproto::signaling::SignalingMessage>(_msg);
    auto& rtc_msg = msg->rtc_message();
    LOG(DEBUG) << "Received signaling key:" << msg->rtc_message().key().c_str()
               << ", value:" << msg->rtc_message().value().c_str();
    tp_client_->onSignalingMessage(rtc_msg.key().c_str(), rtc_msg.value().c_str());
}

void Client::dispatchSignalingMessageCore(std::shared_ptr<google::protobuf::MessageLite> _msg) {
    auto msg = std::static_pointer_cast<ltproto::signaling::SignalingMessage>(_msg);
    auto& coremsg = msg->core_message();
    LOG(DEBUG) << "Dispatch signaling core message: " << coremsg.key();
    if (coremsg.key() == kSigCoreClose) {
        sdl_->stop();
    }
}

void Client::sendKeepaliveToSignalingServer() {
    auto msg = std::make_shared<ltproto::common::KeepAlive>();
    signaling_client_->send(ltproto::id(msg), msg);
    postDelayTask(10'000, std::bind(&Client::sendKeepaliveToSignalingServer, this));
}

bool Client::initTransport() {
    switch (LT_TRANSPORT_TYPE) {
    case LT_TRANSPORT_TCP:
        tp_client_ = createTcpClient();
        break;
    case LT_TRANSPORT_RTC:
        tp_client_ = createRtcClient();
        break;
    case LT_TRANSPORT_RTC2:
        tp_client_ = createRtc2Client();
        break;
    default:
        break;
    }
    if (tp_client_ == nullptr) {
        LOG(ERR) << "Create lt::tp::Client failed";
        return false;
    }

    if (!tp_client_->connect()) {
        LOG(INFO) << "lt::tp::Client connect failed";
        return false;
    }
    return true;
}

tp::Client* Client::createTcpClient() {
    namespace ph = std::placeholders;
    lt::tp::ClientTCP::Params params{};
    params.user_data = this;
    params.on_data = &Client::onTpData;
    params.on_video = &Client::onTpVideoFrame;
    params.on_audio = &Client::onTpAudioData;
    params.on_connected = &Client::onTpConnected;
    params.on_failed = &Client::onTpFailed;
    params.on_disconnected = &Client::onTpDisconnected;
    params.on_signaling_message = &Client::onTpSignalingMessage;
    params.video_codec_type = video_params_.codec_type;
    // FIXME: 修改TCP接口
    auto client = lt::tp::ClientTCP::create(params);
    return client.release();
}

tp::Client* Client::createRtcClient() {
    namespace ph = std::placeholders;
    rtc::Client::Params params{};
    params.user_data = this;
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
    params.on_data = &Client::onTpData;
    params.on_video = &Client::onTpVideoFrame;
    params.on_audio = &Client::onTpAudioData;
    params.on_connected = &Client::onTpConnected;
    params.on_conn_changed = &Client::onTpConnChanged;
    params.on_failed = &Client::onTpFailed;
    params.on_disconnected = &Client::onTpDisconnected;
    params.on_signaling_message = &Client::onTpSignalingMessage;
    params.video_codec_type = video_params_.codec_type;
    params.audio_channels = audio_params_.channels;
    params.audio_sample_rate = audio_params_.frames_per_second;
    return rtc::Client::create(std::move(params));
}

tp::Client* Client::createRtc2Client() {
    namespace ph = std::placeholders;
    rtc2::Client::Params params{};
    params.user_data = this;
    params.on_data = &Client::onTpData;
    params.on_video = &Client::onTpVideoFrame;
    params.on_audio = &Client::onTpAudioData;
    params.on_connected = &Client::onTpConnected;
    params.on_conn_changed = &Client::onTpConnChanged;
    params.on_failed = &Client::onTpFailed;
    params.on_disconnected = &Client::onTpDisconnected;
    params.on_signaling_message = &Client::onTpSignalingMessage;
    params.audio_recv_ssrc = 687154681;
    params.video_recv_ssrc = 541651314;
    // TODO: key and cert合理的创建时机
    params.key_and_cert = rtc2::KeyAndCert::create();
    params.remote_digest;
    // FIXME: 修改rtc2接口
    auto client = rtc2::Client::create(params);
    return client.release();
}

void Client::onTpData(void* user_data, const uint8_t* data, uint32_t size, bool is_reliable) {
    auto that = reinterpret_cast<Client*>(user_data);
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
    that->dispatchRemoteMessage(*type, msg);
}

void Client::onTpVideoFrame(void* user_data, const lt::VideoFrame& frame) {
    auto that = reinterpret_cast<Client*>(user_data);
    std::lock_guard lock{that->dr_mutex_};
    if (that->video_pipeline_ == nullptr) {
        return;
    }
    VideoDecodeRenderPipeline::Action action = that->video_pipeline_->submit(frame);
    switch (action) {
    case VideoDecodeRenderPipeline::Action::REQUEST_KEY_FRAME:
    {
        auto req = std::make_shared<ltproto::client2worker::RequestKeyframe>();
        that->sendMessageToHost(ltproto::id(req), req, true);
        break;
    }
    case VideoDecodeRenderPipeline::Action::NONE:
        break;
    default:
        break;
    }
}

void Client::onTpAudioData(void* user_data, const lt::AudioData& audio_data) {
    auto that = reinterpret_cast<Client*>(user_data);
    // FIXME: transport在audio_player_实例化前，不应回调audio数据
    if (that->audio_player_) {
        that->audio_player_->submit(audio_data.data, audio_data.size);
    }
}

void Client::onTpConnected(void* user_data, lt::LinkType link_type) {
    auto that = reinterpret_cast<Client*>(user_data);
    (void)link_type;
    that->video_pipeline_ = VideoDecodeRenderPipeline::create(that->video_params_);
    if (that->video_pipeline_ == nullptr) {
        LOG(ERR) << "Create VideoDecodeRenderPipeline failed";
        return;
    }
    that->input_params_.send_message =
        std::bind(&Client::sendMessageToHost, that, std::placeholders::_1, std::placeholders::_2,
                  std::placeholders::_3);
    that->input_params_.host_height = that->video_params_.height;
    that->input_params_.host_width = that->video_params_.width;
    that->input_params_.toggle_fullscreen = std::bind(&Client::toggleFullscreen, that);
    that->input_capturer_ = InputCapturer::create(that->input_params_);
    if (that->input_capturer_ == nullptr) {
        LOG(ERR) << "Create InputCapturer failed";
        return;
    }
    that->audio_player_ = AudioPlayer::create(that->audio_params_);
    if (that->audio_player_ == nullptr) {
        LOG(INFO) << "Create AudioPlayer failed";
        return;
    }
    that->hb_thread_->post(std::bind(&Client::sendKeepAlive, that));
    // 如果未来有“串流”以外的业务，在这个StartTransmission添加字段.
    auto start = std::make_shared<ltproto::client2worker::StartTransmission>();
    start->set_client_os(ltproto::client2worker::StartTransmission_ClientOS_Windows);
    start->set_token(that->auth_token_);
    that->sendMessageToHost(ltproto::id(start), start, true);
    that->postTask(std::bind(&Client::syncTime, that));

    // setTitle
    that->is_p2p_ = link_type != lt::LinkType::RelayUDP;
    std::ostringstream oss;
    oss << "Lanthing " << (that->is_p2p_.value() ? "P2P " : "Relay ")
        << toString(that->video_params_.codec_type) << " GPU:GPU"; // 暂时只支持硬件编解码.
    that->sdl_->setTitle(oss.str());
}

void Client::onTpConnChanged(void*) {}

void Client::onTpFailed(void* user_data) {
    auto that = reinterpret_cast<Client*>(user_data);
    that->stopWait();
}

void Client::onTpDisconnected(void* user_data) {
    auto that = reinterpret_cast<Client*>(user_data);
    that->stopWait();
}

void Client::onTpSignalingMessage(void* user_data, const char* key, const char* value) {
    auto that = reinterpret_cast<Client*>(user_data);
    // 将key和value封装在proto里.
    auto msg = std::make_shared<ltproto::signaling::SignalingMessage>();
    msg->set_level(ltproto::signaling::SignalingMessage::Rtc);
    auto rtc_msg = msg->mutable_rtc_message();
    rtc_msg->set_key(key);
    rtc_msg->set_value(value);
    that->postTask([that, msg]() { that->signaling_client_->send(ltproto::id(msg), msg); });
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
    case ltproto::type::kSendSideStat:
        onSendSideStat(msg);
        break;
    default:
        LOG(WARNING) << "Unknown message type: " << type;
        break;
    }
}

void Client::sendKeepAlive() {
    auto keep_alive = std::make_shared<ltproto::common::KeepAlive>();
    sendMessageToHost(ltproto::id(keep_alive), keep_alive, true);

    const auto k500ms = ltlib::TimeDelta{500'000};
    hb_thread_->post_delay(k500ms, std::bind(&Client::sendKeepAlive, this));
}

bool Client::sendMessageToHost(uint32_t type,
                               const std::shared_ptr<google::protobuf::MessageLite>& msg,
                               bool reliable) {
    auto packet = ltproto::Packet::create({type, msg}, false);
    if (!packet.has_value()) {
        LOG(ERR) << "Create ltproto::Packet failed, type:" << type;
        return false;
    }
    const auto& pkt = packet.value();
    // WebRTC的数据通道可以帮助我们完成stream->packet的过程，所以这里不需要把packet
    // header一起传过去.
    bool success = tp_client_->sendData(pkt.payload.get(), pkt.header.payload_size, reliable);
    return success;
}

void Client::onStartTransmissionAck(const std::shared_ptr<google::protobuf::MessageLite>& _msg) {
    auto msg = std::static_pointer_cast<ltproto::client2worker::StartTransmissionAck>(_msg);
    if (msg->err_code() == ltproto::ErrorCode::Success) {
        LOG(INFO) << "Received StartTransmissionAck with success";
    }
    else {
        LOG(INFO) << "StartTransmission failed with " << ltproto::ErrorCode_Name(msg->err_code());
        stopWait();
    }
}

void Client::onTimeSync(std::shared_ptr<google::protobuf::MessageLite> _msg) {
    auto msg = std::static_pointer_cast<ltproto::client2service::TimeSync>(_msg);
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

void Client::onSendSideStat(std::shared_ptr<google::protobuf::MessageLite> _msg) {
    auto msg = std::static_pointer_cast<ltproto::client2worker::SendSideStat>(_msg);
    video_pipeline_->setNack(static_cast<uint32_t>(msg->nack()));
    video_pipeline_->setBWE(static_cast<uint32_t>(msg->bwe()));
}

} // namespace cli

} // namespace lt