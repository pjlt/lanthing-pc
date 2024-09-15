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

#include <filesystem>
#include <sstream>

#include <ltproto/client2app/client_status.pb.h>
#include <ltproto/client2service/time_sync.pb.h>
#include <ltproto/client2worker/change_streaming_params.pb.h>
#include <ltproto/client2worker/change_streaming_params_ack.pb.h>
#include <ltproto/client2worker/cursor_info.pb.h>
#include <ltproto/client2worker/request_keyframe.pb.h>
#include <ltproto/client2worker/send_side_stat.pb.h>
#include <ltproto/client2worker/start_transmission.pb.h>
#include <ltproto/client2worker/start_transmission_ack.pb.h>
#include <ltproto/client2worker/switch_mouse_mode.pb.h>
#include <ltproto/common/keep_alive.pb.h>
#include <ltproto/ltproto.h>
#include <ltproto/server/request_connection.pb.h>
#include <ltproto/server/request_connection_ack.pb.h>
#include <ltproto/signaling/join_room.pb.h>
#include <ltproto/signaling/join_room_ack.pb.h>
#include <ltproto/signaling/signaling_message.pb.h>
#include <ltproto/signaling/signaling_message_ack.pb.h>

#include <lt_constants.h>
#include <ltlib/logging.h>
#include <ltlib/system.h>
#include <ltlib/time_sync.h>

#include <transport/transport_rtc.h>
// #include <transport/transport_rtc2.h>
#include <transport/transport_tcp.h>

namespace {

lt::AudioCodecType atype(int32_t transport_type) {
    switch (transport_type) {
    case ltproto::common::TransportType::RTC:
        return lt::AudioCodecType::PCM;
    case ltproto::common::TransportType::RTC2:
        return lt::AudioCodecType::PCM;
    case ltproto::common::TransportType::TCP:
        return lt::AudioCodecType::OPUS;
    default:
        LOG(FATAL) << "Unknown transport type";
        return lt::AudioCodecType::OPUS;
    }
}

std::string toTitle(lt::LinkType type) {
    switch (type) {
    case lt::LinkType::UDP:
    case lt::LinkType::LanUDP:
    case lt::LinkType::WanUDP:
    case lt::LinkType::IPv6UDP:
        return "P2P";
    case lt::LinkType::RelayUDP:
        return "Relay";
    case lt::LinkType::TCP:
        return "TCP";
    case lt::LinkType::Unknown:
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
        options.find("-afreq") == options.end() || options.find("-rotation") == options.end() ||
        options.find("-trans") == options.end()) {
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
    int32_t transport_type = std::atoi(options["-trans"].c_str());
    int32_t signaling_port = std::atoi(options["-port"].c_str());
    params.codec = options["-codec"];
    int32_t width = std::atoi(options["-width"].c_str());
    int32_t height = std::atoi(options["-height"].c_str());
    int32_t freq = std::atoi(options["-freq"].c_str());
    int32_t audio_freq = std::atoi(options["-afreq"].c_str());
    int32_t audio_channels = std::atoi(options["-chans"].c_str());
    int32_t rotation = std::atoi(options["-rotation"].c_str());
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
    if (transport_type != ltproto::common::TransportType::RTC &&
        transport_type != ltproto::common::TransportType::TCP) {
        LOG(ERR) << "Invalid parameter: trans " << transport_type;
        return nullptr;
    }
    params.transport_type = transport_type;

    if (signaling_port <= 0 || signaling_port > 65535) {
        LOG(ERR) << "Invalid parameter: port " << signaling_port;
        return nullptr;
    }
    params.signaling_port = static_cast<uint16_t>(signaling_port);

    if (width <= 0) {
        LOG(ERR) << "Invalid parameter: width " << width;
        return nullptr;
    }
    params.width = static_cast<uint32_t>(width);

    if (height <= 0) {
        LOG(ERR) << "Invalid parameter: height " << height;
        return nullptr;
    }
    params.height = static_cast<uint32_t>(height);

    if (freq <= 0) {
        LOG(ERR) << "Invalid parameter: freq " << freq;
        return nullptr;
    }
    params.screen_refresh_rate = static_cast<uint32_t>(freq);

    if (audio_channels <= 0) {
        LOG(ERR) << "Invalid parameter: achans " << audio_channels;
        return nullptr;
    }
    params.audio_channels = static_cast<uint32_t>(audio_channels);

    if (audio_freq <= 0) {
        LOG(ERR) << "Invalid parameter: afreq " << audio_freq;
        return nullptr;
    }
    params.audio_freq = static_cast<uint32_t>(audio_freq);

    if (rotation != 0 && rotation != 90 && rotation != 180 && rotation != 270) {
        LOG(ERR) << "Invalid parameter: rotation " << rotation;
        return nullptr;
    }
    params.rotation = static_cast<uint32_t>(rotation);
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
    , video_params_{videoCodecType(params.codec.c_str()),
                    videoCodecType(params.codec.c_str()),
                    params.width,
                    params.height,
                    params.screen_refresh_rate,
                    params.rotation,
                    is_stretch_,
                    std::bind(&Client::sendMessageToHost, this, std::placeholders::_1,
                              std::placeholders::_2, std::placeholders::_3),
                    std::bind(&Client::onUserSwitchStretch, this),
                    std::bind_front(&Client::resetVideoPipeline, this)}
    , audio_params_{atype(params.transport_type), params.audio_freq, params.audio_channels}
    , reflex_servers_{params.reflex_servers}
    , transport_type_{params.transport_type} {
    if (video_params_.decode_codec == VideoCodecType::H264_420_SOFT) {
        video_params_.decode_codec = VideoCodecType::H264_420;
    }
}

Client::~Client() {
    {
        std::lock_guard lock{ioloop_mutex_};
        stoped_ = true;
        signaling_client_.reset();
        app_client_.reset();
        ioloop_.reset();
    }
    if (tp_client_ != nullptr) {
        switch (transport_type_) {
        case ltproto::common::TransportType::TCP:
        {
            auto tcp_cli = static_cast<lt::tp::ClientTCP*>(tp_client_);
            delete tcp_cli;
            break;
        }
        case ltproto::common::TransportType::RTC:
        { // rtc.dll build on another machine!
            rtc::Client::destroy(tp_client_);
            break;
        }
        case ltproto::common::TransportType::RTC2:
        {
            // auto rtc2_cli = static_cast<rtc2::Client*>(tp_client_);
            // delete rtc2_cli;
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
    video_params_.status_color = settings_->getInteger("status_color").value_or(-1);
    absolute_mouse_ = settings_->getBoolean("absolute_mouse").value_or(true);
    video_params_.absolute_mouse = absolute_mouse_;
    ioloop_ = ltlib::IOLoop::create();
    if (ioloop_ == nullptr) {
        LOG(ERR) << "Init IOLoop failed";
        return false;
    }
    if (!initSignalingClient()) {
        LOG(ERR) << "Create signaling client failed";
        return false;
    }
    if (!initAppClient()) {
        LOG(ERR) << "Create app client failed";
        return false;
    }
    lt::plat::PcSdl::Params sdl_params{};
    sdl_params.on_reset = std::bind(&Client::onPlatformRenderTargetReset, this);
    sdl_params.windowed_fullscreen = windowed_fullscreen_;
    sdl_params.absolute_mouse = absolute_mouse_;
    sdl_ = lt::plat::PcSdl::create(sdl_params);
    if (sdl_ == nullptr) {
        LOG(INFO) << "Create SDL failed";
        return false;
    }
    LOG(INFO) << "Create SDL success";
    sdl_->setTitle("Joining room....");
    io_thread_ = ltlib::BlockingThread::create(
        "io_thread", [this](const std::function<void()>& i_am_alive) { ioLoop(i_am_alive); });
    return true;
}

bool Client::initSettings() {
    settings_ = ltlib::Settings::create(ltlib::Settings::Storage::Sqlite);
    return settings_ != nullptr;
}

#define MACRO_TO_STRING_HELPER(str) #str
#define MACRO_TO_STRING(str) MACRO_TO_STRING_HELPER(str)
#include <trusted-root.cert>
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

bool Client::initAppClient() {
    ltlib::Client::Params params{};
    params.stype = ltlib::StreamType::Pipe;
    params.ioloop = ioloop_.get();
#if LT_WINDOWS
    params.pipe_name = "\\\\?\\pipe\\lanthing_client_manager";
#else
    std::filesystem::path fs = ltlib::getConfigPath(false);
    fs = fs / "pipe_lanthing_client_manager";
    params.pipe_name = fs.string();
#endif
    params.is_tls = false;
    params.on_connected = std::bind(&Client::onAppConnected, this);
    params.on_closed = std::bind(&Client::onAppDisconnected, this);
    params.on_reconnecting = std::bind(&Client::onAppReconnecting, this);
    params.on_message =
        std::bind(&Client::onAppMessage, this, std::placeholders::_1, std::placeholders::_2);
    app_client_ = ltlib::Client::create(params);
    return app_client_ != nullptr;
}

int32_t Client::loop() {
    int32_t ret = sdl_->loop();
    struct ExitCtx {
        std::condition_variable cv;
        std::mutex mtx;
        std::atomic<bool> msg_sent = false;
    };
    auto exit_ctx = std::make_shared<ExitCtx>();
    auto weak_ctx = std::weak_ptr{exit_ctx};
    auto msg = std::make_shared<ltproto::signaling::SignalingMessage>();
    msg->set_level(ltproto::signaling::SignalingMessage_Level_Core);
    auto coremsg = msg->mutable_core_message();
    coremsg->set_key(kSigCoreClose);
    // 退出进程前，向主机端发送kSigCoreClose
    postTask([this, msg, weak_ctx]() {
        signaling_client_->send(ltproto::id(msg), msg, [weak_ctx]() {
            auto ctx = weak_ctx.lock();
            if (ctx == nullptr) {
                return;
            }
            {
                std::lock_guard lg{ctx->mtx};
                ctx->msg_sent = true;
            }
            ctx->cv.notify_one();
        });
    });
    // 等待发送成功或超时
    std::unique_lock lock{exit_ctx->mtx};
    exit_ctx->cv.wait_for(lock, std::chrono::milliseconds{50},
                          [exit_ctx]() { return exit_ctx->msg_sent.load(); });
    // 运行到此处，要么是50ms超时，要么是libuv告诉我们kSigCore发送成功
    // 但我并不清楚这个"发送成功"是指内容已经刷到缓冲区，还是指已经发送到网络
    // 另外，即便是已经发送到网络，还是有丢包重传的可能
    // 由于暂未做所谓的"优雅关闭"，这里sleep一会，一定程度减小'关闭'信令发不到对面的概率
    // 最后最后，对面就算收不到这个关闭信令，也有超时关闭的逻辑
    std::this_thread::sleep_for(std::chrono::milliseconds{50});
    return ret;
}

void Client::ioLoop(const std::function<void()>& i_am_alive) {
    LOG(INFO) << "Lanthing client enter io loop";
    ioloop_->run(i_am_alive);
    LOG(INFO) << "Lanthing client exit io loop";
}

void Client::onPlatformRenderTargetReset() {
    // NOTE: 这运行在platform线程
    std::lock_guard lock{dr_mutex_};
    // video_pipeline_.reset();
    // video_pipeline_ = VideoDecodeRenderPipeline::create(video_params_);
    // if (video_pipeline_ == nullptr) {
    //     LOG(ERR) << "Create VideoDecodeRenderPipeline failed";
    // }
    if (video_pipeline_ != nullptr) {
        video_pipeline_->resetRenderTarget();
    }
}

void Client::postTask(const std::function<void()>& task) {
    // 参考Service::postTask
    while (!ioloop_mutex_.try_lock_shared()) {
        if (stoped_) {
            return;
        }
    }
    if (ioloop_) {
        ioloop_->post(task);
    }
    ioloop_mutex_.unlock_shared();
}

void Client::postDelayTask(int64_t delay_ms, const std::function<void()>& task) {
    // 参考Service::postTask
    while (!ioloop_mutex_.try_lock_shared()) {
        if (stoped_) {
            return;
        }
    }
    if (ioloop_) {
        ioloop_->postDelay(delay_ms, task);
    }
    ioloop_mutex_.unlock_shared();
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

void Client::switchMouseMode() {
    absolute_mouse_ = !absolute_mouse_;
    sdl_->switchMouseMode(absolute_mouse_);
    bool switched = false;
    {
        std::lock_guard lock{dr_mutex_};
        if (video_pipeline_) {
            video_pipeline_->switchMouseMode(absolute_mouse_);
            switched = true;
        }
    }
    if (switched) {
        auto msg = std::make_shared<ltproto::client2worker::SwitchMouseMode>();
        msg->set_absolute(absolute_mouse_);
        sendMessageToHost(ltproto::id(msg), msg, true);
    }
}

void Client::checkWorkerTimeout() {
    constexpr int64_t kFiveSeconds = 5'000;
    constexpr int64_t k500ms = 500;
    auto now = ltlib::steady_now_ms();
    if (now - last_received_keepalive_ > kFiveSeconds) {
        LOG(INFO) << "Didn't receive KeepAliveAck from worker for "
                  << (now - last_received_keepalive_) << "ms, exit";
        tellAppKeepAliveTimeout();
        // 为了让消息发送到app，延迟50ms再关闭程序
        postDelayTask(50, [this]() { sdl_->stop(); });
        return;
    }
    postDelayTask(k500ms, std::bind(&Client::checkWorkerTimeout, this));
}

void Client::tellAppKeepAliveTimeout() {
    if (connected_to_app_) {
        auto msg = std::make_shared<ltproto::client2app::ClientStatus>();
        msg->set_status(ltproto::ErrorCode::ClientStatusKeepAliveTimeout);
        app_client_->send(ltproto::id(msg), msg);
    }
    else {
        LOG(WARNING) << "Not connected to app, won't send ClientStatus";
    }
}

void Client::onAppConnected() {
    LOG(INFO) << "Connected to app";
    connected_to_app_ = true;
}

void Client::onAppDisconnected() {
    LOG(ERR) << "Disconnected from app, won't reconnect again";
    connected_to_app_ = false;
}

void Client::onAppReconnecting() {
    LOG(INFO) << "Reconnecting to app...";
    connected_to_app_ = false;
}

void Client::onAppMessage(uint32_t type, std::shared_ptr<google::protobuf::MessageLite> msg) {
    namespace ltype = ltproto::type;
    switch (type) {
    case ltype::kClipboard:
        // TODO: 考虑一下会不会死锁
        sendMessageToHost(type, msg, true);
        break;
    case ltype::kPullFile:
    case ltype::kFileChunk:
    case ltype::kFileChunkAck:
        // TODO: 此处应验证msg的deviceid，但由于我们的Client没有记录对端的
        // deviceid，暂时没法验证
        sendMessageToHost(type, msg, true);
        break;
    default:
        LOG(WARNING) << "Received unknown message from app, type " << type;
        break;
    }
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
    LOG(INFO) << "Disconnected from signaling server, exit process";
    sdl_->stop();
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
    // initTransport必须在sdl_创建之后调用，因为onTpConnected会用到sdl_
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
    switch (transport_type_) {
    case ltproto::common::TransportType::TCP:
        tp_client_ = createTcpClient();
        break;
    case ltproto::common::TransportType::RTC:
        tp_client_ = createRtcClient();
        break;
    case ltproto::common::TransportType::RTC2:
        // tp_client_ = createRtc2Client();
        // break;
    default:
        LOG(ERR) << "initTransport failed: Unknown transport type " << transport_type_;
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
    uint16_t min_port = static_cast<uint16_t>(settings_->getInteger("min_port").value_or(0));
    uint16_t max_port = static_cast<uint16_t>(settings_->getInteger("max_port").value_or(0));
    ignored_nic_ = settings_->getString("ignored_nic").value_or("");
    if (params.use_nbp2p) {
        params.nbp2p_params.disable_ipv6 = false;
        params.nbp2p_params.disable_lan_udp = false;
        params.nbp2p_params.disable_mapping = false;
        params.nbp2p_params.disable_reflex = false;
        params.nbp2p_params.disable_relay = false;
        params.nbp2p_params.min_port = min_port;
        params.nbp2p_params.max_port = max_port;
        params.nbp2p_params.username = p2p_username_.c_str();
        params.nbp2p_params.password = p2p_password_.c_str();
        params.nbp2p_params.reflex_servers = reflex_servers.data();
        params.nbp2p_params.reflex_servers_count = static_cast<uint32_t>(reflex_servers.size());
        params.nbp2p_params.relay_servers = nullptr;
        params.nbp2p_params.relay_servers_count = 0;
        params.nbp2p_params.ignored_adapters = ignored_nic_.c_str();
    }
    params.on_data = &Client::onTpData;
    params.on_video = &Client::onTpVideoFrame;
    params.on_audio = &Client::onTpAudioData;
    params.on_connected = &Client::onTpConnected;
    params.on_conn_changed = &Client::onTpConnChanged;
    params.on_failed = &Client::onTpFailed;
    params.on_disconnected = &Client::onTpDisconnected;
    params.on_signaling_message = &Client::onTpSignalingMessage;
    params.video_codec_type = video_params_.decode_codec;
    params.audio_channels = audio_params_.channels;
    params.audio_sample_rate = audio_params_.frames_per_second;
    return rtc::Client::create(std::move(params));
}

/*
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
    // params.remote_digest;
    // FIXME: 修改rtc2接口
    auto client = rtc2::Client::create(params);
    return client.release();
}
*/

void Client::onTpData(void* user_data, const uint8_t* data, uint32_t size, bool is_reliable) {
    // 跑在数据通道线程
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
    // 跑在video线程
    auto that = reinterpret_cast<Client*>(user_data);
    video::DecodeRenderPipeline::Action action = video::DecodeRenderPipeline::Action::NONE;
    {
        std::lock_guard lock{that->dr_mutex_};
        if (that->video_pipeline_ == nullptr) {
            return;
        }
        action = that->video_pipeline_->submit(frame);
    }
    switch (action) {
    case video::DecodeRenderPipeline::Action::REQUEST_KEY_FRAME:
    {
        auto req = std::make_shared<ltproto::client2worker::RequestKeyframe>();
        that->sendMessageToHost(ltproto::id(req), req, true);
        break;
    }
    case video::DecodeRenderPipeline::Action::NONE:
        break;
    default:
        break;
    }
}

void Client::onTpAudioData(void* user_data, const lt::AudioData& audio_data) {
    // 跑在audio线程
    auto that = reinterpret_cast<Client*>(user_data);
    // FIXME: transport在audio_player_实例化前，不应回调audio数据
    if (that->audio_player_) {
        that->audio_player_->submit(audio_data.data, audio_data.size);
    }
}

void Client::onTpConnected(void* user_data, lt::LinkType link_type) {
    // 跑在数据通道线程
    LOG(INFO) << "Connected, LinkType " << toString(link_type);
    auto that = reinterpret_cast<Client*>(user_data);
    that->video_params_.sdl = that->sdl_.get();
    that->input_params_.sdl = that->sdl_.get();
    if (that->video_device_ == nullptr) {
        that->video_device_ = plat::VideoDevice::create(that->video_params_.decode_codec);
        if (that->video_device_ == nullptr &&
            that->video_params_.decode_codec == VideoCodecType::H264_420) {
            that->video_params_.decode_codec = VideoCodecType::H264_420_SOFT;
            that->video_device_ = plat::VideoDevice::create(VideoCodecType::H264_420_SOFT);
        }
    }
    if (that->video_device_ == nullptr) {
        LOG(ERR) << "Create VideoDevice failed";
        return;
    }
    that->video_params_.device = that->video_device_->device();
    that->video_params_.context = that->video_device_->context();
    // 将任务post到主线程
    that->video_pipeline_ = video::DecodeRenderPipeline::create(that->video_params_);
    if (that->video_pipeline_ == nullptr) {
        LOG(ERR) << "Create VideoDecodeRenderPipeline failed";
        return;
    }
    int64_t rel_mouse_accel = that->settings_->getInteger("rel_mouse_accel").value_or(0);
    that->input_params_.send_message =
        std::bind(&Client::sendMessageToHost, that, std::placeholders::_1, std::placeholders::_2,
                  std::placeholders::_3);
    that->input_params_.video_height = that->video_params_.height;
    that->input_params_.video_width = that->video_params_.width;
    that->input_params_.rotation = that->video_params_.rotation;
    that->input_params_.stretch = that->video_params_.stretch;
    that->input_params_.rel_mouse_accel = rel_mouse_accel;
    that->input_params_.toggle_fullscreen = std::bind(&Client::toggleFullscreen, that);
    that->input_params_.switch_mouse_mode = std::bind(&Client::switchMouseMode, that);
    that->input_capturer_ = input::Capturer::create(that->input_params_);
    if (that->input_capturer_ == nullptr) {
        LOG(ERR) << "Create InputCapturer failed";
        return;
    }
    that->audio_player_ = audio::Player::create(that->audio_params_);
    if (that->audio_player_ == nullptr) {
        LOG(INFO) << "Create AudioPlayer failed";
        return;
    }
    // 心跳检测
    that->postTask(std::bind(&Client::sendKeepAlive, that));
    that->last_received_keepalive_ = ltlib::steady_now_ms();
    that->postDelayTask(500, std::bind(&Client::checkWorkerTimeout, that));
    // 如果未来有“串流”以外的业务，在这个StartTransmission添加字段.
    auto start = std::make_shared<ltproto::client2worker::StartTransmission>();
    start->set_client_os(ltproto::client2worker::StartTransmission_ClientOS_Windows);
    start->set_token(that->auth_token_);
    that->sendMessageToHost(ltproto::id(start), start, true);
    that->postTask(std::bind(&Client::syncTime, that));

    // setTitle
    that->link_type_ = link_type;
    auto codec_type = that->video_params_.decode_codec;
    if (codec_type == VideoCodecType::H264_420_SOFT) {
        codec_type = VideoCodecType::H264_420;
    }
    std::ostringstream oss;
    oss << "Lanthing " << toTitle(link_type) << " " << toString(codec_type) << " "
        << (isHard(that->video_params_.decode_codec) ? "GPU" : "CPU")
        << (isHard(that->video_params_.encode_codec) ? ":GPU" : ":CPU");
    that->sdl_->setTitle(oss.str());
}

void Client::onTpConnChanged(void* user_data, lt::LinkType old_type, lt::LinkType new_type) {
    auto that = reinterpret_cast<Client*>(user_data);
    LOG(INFO) << "Transport LinkType changed: " << toString(old_type) << " => "
              << toString(new_type);
    that->link_type_ = new_type;
    auto codec_type = that->video_params_.decode_codec;
    if (codec_type == VideoCodecType::H264_420_SOFT) {
        codec_type = VideoCodecType::H264_420;
    }
    std::ostringstream oss;
    oss << "Lanthing " << toTitle(new_type) << " " << toString(codec_type) << " "
        << (isHard(that->video_params_.decode_codec) ? "GPU" : "CPU")
        << (isHard(that->video_params_.encode_codec) ? ":GPU" : ":CPU");
    that->sdl_->setTitle(oss.str());
}

void Client::onTpFailed(void* user_data) {
    auto that = reinterpret_cast<Client*>(user_data);
    that->sdl_->stop();
}

void Client::onTpDisconnected(void* user_data) {
    auto that = reinterpret_cast<Client*>(user_data);
    that->sdl_->stop();
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
    case ltproto::type::kKeepAliveAck:
        onKeepAliveAck();
        break;
    case ltproto::type::kStartTransmissionAck:
        onStartTransmissionAck(msg);
        break;
    case ltproto::type::kTimeSync:
        onTimeSync(msg);
        break;
    case ltproto::type::kSendSideStat:
        onSendSideStat(msg);
        break;
    case ltproto::type::kCursorInfo:
        onCursorInfo(msg);
        break;
    case ltproto::type::kChangeStreamingParams:
        onChangeStreamingParams(msg);
        break;
    case ltproto::type::kClipboard:
        onRemoteClipboard(msg);
        break;
    case ltproto::type::kPullFile:
        onRemotePullFile(msg);
        break;
    case ltproto::type::kFileChunk:
        onRemoteFileChunk(msg);
        break;
    case ltproto::type::kFileChunkAck:
        onRemoteFileChunkAck(msg);
        break;
    default:
        LOG(WARNING) << "Unknown message type: " << type;
        break;
    }
}

void Client::sendKeepAlive() {
    auto keep_alive = std::make_shared<ltproto::common::KeepAlive>();
    sendMessageToHost(ltproto::id(keep_alive), keep_alive, true);

    constexpr int64_t k500ms = 500;
    postDelayTask(k500ms, std::bind(&Client::sendKeepAlive, this));
}

void Client::onKeepAliveAck() {
    // Ack是worker回复的，其它消息可能是service发送的，我们的目的是判断worker还在不在，所以只用KeepAliveAck来更新时间
    last_received_keepalive_ = ltlib::steady_now_ms();
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
        auto switch_mouse = std::make_shared<ltproto::client2worker::SwitchMouseMode>();
        switch_mouse->set_absolute(absolute_mouse_);
        sendMessageToHost(ltproto::id(switch_mouse), switch_mouse, true);
    }
    else {
        LOG(INFO) << "StartTransmission failed with " << ltproto::ErrorCode_Name(msg->err_code());
        sdl_->stop();
    }
}

void Client::onTimeSync(std::shared_ptr<google::protobuf::MessageLite> _msg) {
    auto msg = std::static_pointer_cast<ltproto::client2service::TimeSync>(_msg);
    auto result = time_sync_.calc(msg->t0(), msg->t1(), msg->t2(), ltlib::steady_now_us());
    if (result.has_value()) {
        rtt_ = result->rtt;
        time_diff_ = result->time_diff;
        LOG(DEBUG) << "rtt:" << rtt_ << ", time_diff:" << time_diff_;
        {
            std::lock_guard lock{dr_mutex_};
            if (video_pipeline_) {
                video_pipeline_->setTimeDiff(time_diff_);
                video_pipeline_->setRTT(rtt_);
            }
        }
    }
}

void Client::onSendSideStat(std::shared_ptr<google::protobuf::MessageLite> _msg) {
    auto msg = std::static_pointer_cast<ltproto::client2worker::SendSideStat>(_msg);
    std::lock_guard lock{dr_mutex_};
    if (video_pipeline_) {
        video_pipeline_->setNack(static_cast<uint32_t>(msg->nack()));
        video_pipeline_->setBWE(static_cast<uint32_t>(msg->bwe()));
        video_pipeline_->setLossRate(msg->loss_rate());
    }
}

void Client::onCursorInfo(std::shared_ptr<google::protobuf::MessageLite> _msg) {
    auto msg = std::static_pointer_cast<ltproto::client2worker::CursorInfo>(_msg);
    LOGF(DEBUG, "onCursorInfo id:%d, w:%d, h:%d, x:%d, y%d", msg->preset(), msg->w(), msg->h(),
         msg->x(), msg->y());
    lt::CursorInfo info{};
    if (msg->has_preset()) {
        info.preset = msg->preset();
    }
    info.screen_w = msg->w();
    info.screen_h = msg->h();
    info.x = msg->x();
    info.y = msg->y();
    info.w = msg->cursor_w();
    info.h = msg->cursor_h();
    info.hot_x = msg->hot_x();
    info.hot_y = msg->hot_y();
    info.data_id = msg->data_id();
    info.pitch = msg->pitch();
    if (msg->has_type()) {
        info.type = static_cast<::lt::CursorDataType>(msg->type());
    }
    {
        std::lock_guard lg{cursor_mtx_};
        auto iter = cursors_.find(msg->data_id());
        if (iter != cursors_.end()) {
            info.data = iter->second;
        }
        else if (!msg->data().empty()) {
            info.data.resize(msg->data().size());
            memcpy(info.data.data(), msg->data().data(), info.data.size());
            cursors_[msg->data_id()] = info.data;
        }
    }
    {
        std::lock_guard lock{dr_mutex_};
        video_pipeline_->setCursorInfo(info);
    }
    sdl_->setCursorInfo(info);
}

void Client::onChangeStreamingParams(std::shared_ptr<google::protobuf::MessageLite> _msg) {
    auto msg = std::static_pointer_cast<ltproto::client2worker::ChangeStreamingParams>(_msg);
    auto width = static_cast<uint32_t>(msg->params().video_width());
    auto height = static_cast<uint32_t>(msg->params().video_height());
    auto rotation = static_cast<uint32_t>(msg->params().rotation());
    LOGF(INFO, "Received ChangeStreamingParams(w:%u, h:%u, r:%u), old is (w:%u, h:%u, r:%u)", width,
         height, rotation, video_params_.width, video_params_.height, video_params_.rotation);
    bool success = true;
    if (video_params_.width != width || video_params_.height != height) {
        video_params_.width = width;
        video_params_.height = height;
        video_params_.rotation = rotation;
        {
            std::lock_guard lg{cursor_mtx_};
            cursors_.clear();
        }
        sdl_->clearCursorInfos();
        input_capturer_->changeVideoParameters(video_params_.width, video_params_.height,
                                               video_params_.rotation, is_stretch_);
        std::lock_guard lock{dr_mutex_};
        video_pipeline_.reset(); // 手动reset再create，保证不同时存在两份VideoDecodeRenderPipeline
        video_pipeline_ = video::DecodeRenderPipeline::create(video_params_);
        if (video_pipeline_ == nullptr) {
            success = false;
            LOG(ERR) << "Recreate VideoDecodeRenderPipeline failed";
        }
    }
    auto ack = std::make_shared<ltproto::client2worker::ChangeStreamingParamsAck>();
    ack->set_err_code(success ? ltproto::ErrorCode::Success
                              : ltproto::ErrorCode::InitDecodeRenderPipelineFailed);
    sendMessageToHost(ltproto::id(ack), ack, true);
}

void Client::onRemoteClipboard(std::shared_ptr<google::protobuf::MessageLite> msg) {
    postTask([this, msg]() {
        if (connected_to_app_) {
            app_client_->send(ltproto::type::kClipboard, msg);
        }
        else {
            LOG(WARNING) << "Not connected to app, won't send Clipboard";
        }
    });
}

void Client::onRemotePullFile(std::shared_ptr<google::protobuf::MessageLite> msg) {
    postTask([this, msg]() {
        if (connected_to_app_) {
            app_client_->send(ltproto::type::kPullFile, msg);
        }
        else {
            LOG(WARNING) << "Not connected to app, won't send PullFile";
        }
    });
}

void Client::onRemoteFileChunk(std::shared_ptr<google::protobuf::MessageLite> msg) {
    postTask([this, msg]() {
        if (connected_to_app_) {
            app_client_->send(ltproto::type::kFileChunk, msg);
        }
        else {
            LOG(WARNING) << "Not connected to app, won't send FileChunk";
        }
    });
}

void Client::onRemoteFileChunkAck(std::shared_ptr<google::protobuf::MessageLite> msg) {
    postTask([this, msg]() {
        if (connected_to_app_) {
            app_client_->send(ltproto::type::kFileChunkAck, msg);
        }
        else {
            LOG(WARNING) << "Not connected to app, won't send FileChunkAck";
        }
    });
}

void Client::onUserSwitchStretch() {
    // 跑在渲染线程
    LOG(INFO) << "Switching stretch from " << std::boolalpha << is_stretch_.load() << " to "
              << !is_stretch_.load();
    // is_stretch_这个状态只在'Client'里更新，其它地方都是副本
    is_stretch_ = !is_stretch_;
    postTask([this]() {
        // 统一用IOLoop去做，减小bug发生概率
        input_capturer_->changeVideoParameters(video_params_.width, video_params_.height,
                                               video_params_.rotation, is_stretch_);
        std::lock_guard lock{dr_mutex_};
        video_pipeline_->switchStretchMode(is_stretch_);
    });
}

void Client::resetVideoPipeline() {
    // 跑在解码线程
    postTask([this]() {
        bool need_exit = false;
        {
            std::lock_guard lock{dr_mutex_};
            video_pipeline_
                .reset(); // 手动reset再create，保证不同时存在两份VideoDecodeRenderPipeline
            video_pipeline_ = video::DecodeRenderPipeline::create(video_params_);
            if (video_pipeline_ == nullptr) {
                LOG(ERR) << "Recreate VideoDecodeRenderPipeline failed, exit process";
                need_exit = true;
            }
        }
        if (need_exit) {
            sdl_->stop();
        }
    });
}

} // namespace cli

} // namespace lt
