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

#include "worker_session.h"

#include <fstream>

#include <ltlib/logging.h>

#include <ltproto/client2service/time_sync.pb.h>
#include <ltproto/client2worker/audio_data.pb.h>
#include <ltproto/client2worker/change_streaming_params.pb.h>
#include <ltproto/client2worker/mouse_event.pb.h>
#include <ltproto/client2worker/request_keyframe.pb.h>
#include <ltproto/client2worker/send_side_stat.pb.h>
#include <ltproto/client2worker/start_transmission.pb.h>
#include <ltproto/client2worker/start_transmission_ack.pb.h>
#include <ltproto/client2worker/video_frame.pb.h>
#include <ltproto/common/keep_alive.pb.h>
#include <ltproto/common/keep_alive_ack.pb.h>
#include <ltproto/server/open_connection.pb.h>
#include <ltproto/service2app/accepted_connection.pb.h>
#include <ltproto/service2app/connection_status.pb.h>
#include <ltproto/service2app/disconnected_connection.pb.h>
#include <ltproto/signaling/join_room.pb.h>
#include <ltproto/signaling/join_room_ack.pb.h>
#include <ltproto/signaling/signaling_message.pb.h>
#include <ltproto/signaling/signaling_message_ack.pb.h>
#include <ltproto/worker2service/reconfigure_video_encoder.pb.h>
#include <ltproto/worker2service/start_working.pb.h>
#include <ltproto/worker2service/start_working_ack.pb.h>
#include <ltproto/worker2service/stop_working.pb.h>

#include <ltlib/system.h>
#include <ltlib/times.h>

#include <transport/transport_rtc.h>
#include <transport/transport_rtc2.h>
#include <transport/transport_tcp.h>

#include "worker_process.h"
#include <string_keys.h>

namespace {

lt::VideoCodecType to_ltrtc(ltproto::common::VideoCodecType type) {
    switch (type) {
    case ltproto::common::VideoCodecType::AVC:
        return lt::VideoCodecType::H264;
    case ltproto::common::VideoCodecType::HEVC:
        return lt::VideoCodecType::H265;
    default:
        return lt::VideoCodecType::Unknown;
    }
}

} // namespace

namespace lt {

namespace svc {

// 连接流程
// 1. 主控发送RequestConnection到服务器.
// 2. 服务器发送OpenConnection到被控端.
// 3. 被控端连接信令服务器，同时回复服务器OpenConnectionAck
// 4. 服务器回主控端RequestConnectionAck
// 5. 主控端连接信令服务器.
// 6. 主控端连接信令成功，发起rtc连接.

std::shared_ptr<WorkerSession> svc::WorkerSession::create(const Params& params) {
    std::shared_ptr<WorkerSession> session{new WorkerSession(params)};
    if (!session->init(params.msg, params.ioloop)) {
        return nullptr;
    }
    return session;
}

WorkerSession::WorkerSession(const Params& params)
    : session_name_(params.name)
    , post_task_(params.post_task)
    , post_delay_task_(params.post_delay_task)
    , on_accepted_connection_(params.on_accepted_connection)
    , on_connection_status_(params.on_connection_status)
    , user_defined_relay_server_(params.user_defined_relay_server)
    , on_create_session_completed_(params.on_create_completed)
    , on_closed_(params.on_closed)
    , enable_gamepad_(params.enable_gamepad)
    , enable_keyboard_(params.enable_keyboard)
    , enable_mouse_(params.enable_mouse)
    , force_relay_(params.force_relay)
    , min_port_(params.min_port)
    , max_port_(params.max_port) {
    constexpr int kRandLength = 4;
    pipe_name_ = "Lanthing_worker_";
    for (int i = 0; i < kRandLength; ++i) {
        pipe_name_.push_back(rand() % 26 + 'A');
    }
}

WorkerSession::~WorkerSession() {
    if (tp_server_ != nullptr) {
        switch (LT_TRANSPORT_TYPE) {
        case LT_TRANSPORT_TCP:
        {
            auto tcp_svr = static_cast<lt::tp::ServerTCP*>(tp_server_);
            delete tcp_svr;
            break;
        }
        case LT_TRANSPORT_RTC:
        { // rtc.dll build on another machine!
            rtc::Server::destroy(tp_server_);
            break;
        }
        case LT_TRANSPORT_RTC2:
        {
            auto rtc2_svr = static_cast<rtc2::Server*>(tp_server_);
            delete rtc2_svr;
            break;
        }
        default:
            break;
        }
    }
}

void WorkerSession::enableGamepad() {
    enable_gamepad_ = true;
}

void WorkerSession::disableGamepad() {
    enable_gamepad_ = false;
}

void WorkerSession::enableMouse() {
    enable_mouse_ = true;
}

void WorkerSession::disableMouse() {
    enable_mouse_ = false;
}

void WorkerSession::enableKeyboard() {
    enable_keyboard_ = true;
}

void WorkerSession::disableKeyboard() {
    enable_keyboard_ = false;
}

void WorkerSession::close() {
    postTask(std::bind(&WorkerSession::onClosed, this, CloseReason::UserKick));
}

bool WorkerSession::init(std::shared_ptr<google::protobuf::MessageLite> _msg,
                         ltlib::IOLoop* ioloop) {
    auto msg = std::static_pointer_cast<ltproto::server::OpenConnection>(_msg);
    client_device_id_ = msg->client_device_id();
    auth_token_ = msg->auth_token();
    service_id_ = msg->service_id();
    room_id_ = msg->room_id();
    p2p_username_ = msg->p2p_username();
    p2p_password_ = msg->p2p_password();
    signaling_addr_ = msg->signaling_addr();
    signaling_port_ = static_cast<uint16_t>(msg->signaling_port());

    for (int i = 0; i < msg->reflex_servers_size(); i++) {
        reflex_servers_.push_back(msg->reflex_servers(i));
    }

    if (user_defined_relay_server_.empty()) {
        for (int i = 0; i < msg->relay_servers_size(); i++) {
            relay_servers_.push_back(msg->relay_servers(i));
        }
    }
    else {
        relay_servers_.push_back(user_defined_relay_server_);
    }

    if (!msg->has_streaming_params()) {
        // 当前只支持串流，未来有可能支持串流以外的功能，所以这个streaming_params是optional的.
        LOG(WARNING) << "Received OpenConnection without streaming params";
        return false;
    }
    // NOTE: 为了兼容Java，protobuf没有使用uint
    int32_t client_width = msg->streaming_params().video_width();
    int32_t client_height = msg->streaming_params().video_height();
    int32_t client_refresh_rate = msg->streaming_params().screen_refresh_rate();
    if (client_width <= 0 || client_height <= 0 || client_refresh_rate < 0) {
        LOG(ERR) << "Received OpenConnection with invalid streaming params";
        return false;
    }
    std::vector<lt::VideoCodecType> client_codecs;
    for (auto codec : msg->streaming_params().video_codecs()) {
        switch (ltproto::common::VideoCodecType(codec)) {
        case ltproto::common::VideoCodecType::AVC:
            client_codecs.push_back(lt::VideoCodecType::H264);
            break;
        case ltproto::common::VideoCodecType::HEVC:
            client_codecs.push_back(lt::VideoCodecType::H265);
            break;
        default:
            break;
        }
    }
    if (client_codecs.empty()) {
        LOG(WARNING) << "Client doesn't supports any valid video codec";
        return false;
    }

    if (!initSignlingClient(ioloop)) {
        LOG(WARNING) << "Init signaling client failed";
        return false;
    }
    if (!initPipeServer(ioloop)) {
        LOG(WARNING) << "Init worker pipe server failed";
        return false;
    }
    createWorkerProcess((uint32_t)client_width, (uint32_t)client_height,
                        (uint32_t)client_refresh_rate, client_codecs);
    postDelayTask(10'000, std::bind(&WorkerSession::checkAcceptTimeout, this));
    return true;
}

bool WorkerSession::initTransport() {
    switch (LT_TRANSPORT_TYPE) {
    case LT_TRANSPORT_TCP:
        tp_server_ = createTcpServer();
        break;
    case LT_TRANSPORT_RTC:
        tp_server_ = createRtcServer();
        break;
    case LT_TRANSPORT_RTC2:
        tp_server_ = createRtc2Server();
        break;
    default:
        break;
    }
    if (tp_server_ == nullptr) {
        LOG(ERR) << "Create transport server failed";
        return false;
    }
    return true;
}

tp::Server* WorkerSession::createTcpServer() {
    auto negotiated_params =
        std::static_pointer_cast<ltproto::common::StreamingParams>(negotiated_streaming_params_);
    lt::tp::ServerTCP::Params params{};
    params.user_data = this;
    params.video_codec_type = ::to_ltrtc(
        static_cast<ltproto::common::VideoCodecType>(negotiated_params->video_codecs().Get(0)));
    params.on_failed = &WorkerSession::onTpFailed;
    params.on_disconnected = &WorkerSession::onTpDisconnected;
    params.on_accepted = &WorkerSession::onTpAccepted;
    params.on_data = &WorkerSession::onTpData;
    params.on_signaling_message = &WorkerSession::onTpSignalingMessage;
    // FIXME: 修改TCP接口
    auto server = lt::tp::ServerTCP::create(params);
    return server.release();
}

tp::Server* WorkerSession::createRtcServer() {
    auto negotiated_params =
        std::static_pointer_cast<ltproto::common::StreamingParams>(negotiated_streaming_params_);

    rtc::Server::Params params{};
    params.user_data = this;
    params.use_nbp2p = true;
    std::vector<const char*> reflex_servers;
    std::vector<const char*> relay_servers;
    for (auto& svr : reflex_servers_) {
        reflex_servers.push_back(svr.data());
        // LOG(DEBUG) << "Reflex: " << svr;
    }
    for (auto& svr : relay_servers_) {
        relay_servers.push_back(svr.data());
        // LOG(DEBUG) << "Relay: " << svr;
    }
    if (params.use_nbp2p) {
        params.nbp2p_params.disable_ipv6 = force_relay_;
        params.nbp2p_params.disable_lan_udp = force_relay_;
        params.nbp2p_params.disable_mapping = force_relay_;
        params.nbp2p_params.disable_reflex = force_relay_;
        params.nbp2p_params.min_port = min_port_;
        params.nbp2p_params.max_port = max_port_;
        params.nbp2p_params.disable_relay = false;
        params.nbp2p_params.username = p2p_username_.c_str();
        params.nbp2p_params.password = p2p_password_.c_str();
        params.nbp2p_params.reflex_servers = reflex_servers.data();
        params.nbp2p_params.reflex_servers_count = static_cast<uint32_t>(reflex_servers.size());
        params.nbp2p_params.relay_servers = relay_servers.data();
        params.nbp2p_params.relay_servers_count = static_cast<uint32_t>(relay_servers.size());
    }
    params.audio_channels = negotiated_params->audio_channels();
    params.audio_sample_rate = negotiated_params->audio_sample_rate();
    // negotiated_params->video_codecs()的类型居然不是枚举数组
    params.video_codec_type = ::to_ltrtc(
        static_cast<ltproto::common::VideoCodecType>(negotiated_params->video_codecs().Get(0)));
    params.on_failed = &WorkerSession::onTpFailed;
    params.on_disconnected = &WorkerSession::onTpDisconnected;
    params.on_accepted = &WorkerSession::onTpAccepted;
    params.on_data = &WorkerSession::onTpData;
    params.on_signaling_message = &WorkerSession::onTpSignalingMessage;
    params.on_conn_changed = &WorkerSession::onTpConnChanged;
    params.on_keyframe_request = &WorkerSession::onTpRequestKeyframe;
    params.on_video_bitrate_update = &WorkerSession::onTpEesimatedVideoBitreateUpdate;
    params.on_loss_rate_update = &WorkerSession::onTpLossRateUpdate;
    params.on_transport_stat = &WorkerSession::onTpStat;
    return rtc::Server::create(std::move(params));
}

tp::Server* WorkerSession::createRtc2Server() {
    rtc2::Server::Params params{};
    params.user_data = this;
    params.on_failed = &WorkerSession::onTpFailed;
    params.on_disconnected = &WorkerSession::onTpDisconnected;
    params.on_accepted = &WorkerSession::onTpAccepted;
    params.on_data = &WorkerSession::onTpData;
    params.on_signaling_message = &WorkerSession::onTpSignalingMessage;
    params.on_conn_changed = &WorkerSession::onTpConnChanged;
    params.on_keyframe_request = &WorkerSession::onTpRequestKeyframe;
    params.on_video_bitrate_update = &WorkerSession::onTpEesimatedVideoBitreateUpdate;
    params.on_loss_rate_update = &WorkerSession::onTpLossRateUpdate;
    params.remote_digest;
    params.key_and_cert = rtc2::KeyAndCert::create();
    if (params.key_and_cert == nullptr) {
        return nullptr;
    }
    params.video_send_ssrc = 541651314;
    params.audio_send_ssrc = 687154681;
    // FIXME: 修改rtc2接口
    auto server = rtc2::Server::create(params);
    return server.release();
}

void WorkerSession::createWorkerProcess(uint32_t client_width, uint32_t client_height,
                                        uint32_t client_refresh_rate,
                                        std::vector<lt::VideoCodecType> client_codecs) {
    WorkerProcess::Params params{};
    params.pipe_name = pipe_name_;
    params.path = ltlib::getProgramFullpath();
    params.client_width = client_width;
    params.client_height = client_height;
    params.client_refresh_rate = client_refresh_rate;
    params.client_codecs = client_codecs;
    params.on_failed = std::bind(&WorkerSession::onWorkerFailedFromOtherThread, this);
    worker_process_ = WorkerProcess::create(params);
}

void WorkerSession::onClosed(CloseReason reason) {
    // NOTE: 运行在ioloop
    bool rtc_closed = false;
    client_connected_ = false;
    switch (reason) {
    case CloseReason::ClientClose:
        LOG(INFO) << "Close worker session, reason: ClientClose";
        break;
    case CloseReason::WorkerFailed:
        LOG(INFO) << "Close worker session, reason: WorkerFailed";
        break;
    case CloseReason::Timeout:
        LOG(INFO) << "Close worker session, reason: Timeout";
        rtc_closed = true;
        sendSigClose();
        break;
    case CloseReason::UserKick:
        LOG(INFO) << "Close worker session, reason: UserKick";
        sendSigClose();
        break;
    default:
        break;
    }
    if (!rtc_closed && tp_server_ != nullptr) {
        tp_server_->close();
    }
    if (reason != CloseReason::WorkerFailed) {
        auto msg = std::make_shared<ltproto::worker2service::StopWorking>();
        sendToWorker(ltproto::id(msg), msg);
        if (worker_process_ != nullptr) {
            worker_process_->stop();
        }
    }
    postDelayTask(
        100, [this, reason]() { on_closed_(client_device_id_, reason, session_name_, room_id_); });
}

void WorkerSession::maybeOnCreateSessionCompleted() {
    auto empty_params = std::make_shared<ltproto::common::StreamingParams>();
    if (!join_signaling_room_success_.has_value()) {
        return;
    }
    if (join_signaling_room_success_ == false) {
        on_create_session_completed_(false, session_name_, empty_params);
        return;
    }
    if (negotiated_streaming_params_ == nullptr) {
        return;
    }
    if (!initTransport()) {
        on_create_session_completed_(false, session_name_, empty_params);
        return;
    }
    on_create_session_completed_(true, session_name_, negotiated_streaming_params_);
}

void WorkerSession::postTask(const std::function<void()>& task) {
    auto weak_this = weak_from_this();
    post_task_([this, weak_this, task]() {
        auto shared_this = weak_this.lock();
        if (shared_this) {
            task();
        }
    });
}

void WorkerSession::postDelayTask(int64_t delay_ms, const std::function<void()>& task) {
    auto weak_this = weak_from_this();
    post_delay_task_(delay_ms, [this, weak_this, task]() {
        auto shared_this = weak_this.lock();
        if (shared_this) {
            task();
        }
    });
}

#define MACRO_TO_STRING_HELPER(str) #str
#define MACRO_TO_STRING(str) MACRO_TO_STRING_HELPER(str)
#include <trusted-root.cert>
bool WorkerSession::initSignlingClient(ltlib::IOLoop* ioloop) {
    ltlib::Client::Params params{};
    params.stype = ltlib::StreamType::TCP;
    params.ioloop = ioloop;
    params.host = signaling_addr_;
    params.port = signaling_port_;
    params.is_tls = LT_SERVER_USE_SSL;
    params.cert = kLanthingCert;
    params.on_connected = std::bind(&WorkerSession::onSignalingConnected, this);
    params.on_closed = std::bind(&WorkerSession::onSignalingDisconnected, this);
    params.on_reconnecting = std::bind(&WorkerSession::onSignalingReconnecting, this);
    params.on_message = std::bind(&WorkerSession::onSignalingMessageFromNet, this,
                                  std::placeholders::_1, std::placeholders::_2);
    signaling_client_ = ltlib::Client::create(params);
    if (signaling_client_ == nullptr) {
        return false;
    }
    return true;
}
#undef MACRO_TO_STRING
#undef MACRO_TO_STRING_HELPER

void WorkerSession::onSignalingMessageFromNet(uint32_t type,
                                              std::shared_ptr<google::protobuf::MessageLite> msg) {
    namespace ltype = ltproto::type;
    switch (type) {
    case ltype::kKeepAliveAck:
        // do nothing
        break;
    case ltype::kJoinRoomAck:
        onSignalingJoinRoomAck(msg);
        break;
    case ltype::kSignalingMessage:
        onSignalingMessage(msg);
        break;
    case ltype::kSignalingMessageAck:
        onSignalingMessageAck(msg);
        break;
    default:
        LOG(WARNING) << "Unknown signaling message type " << type;
        break;
    }
}

void WorkerSession::onSignalingDisconnected() {
    LOG(INFO) << "Disconnected from signaling srever";
}

void WorkerSession::onSignalingReconnecting() {
    LOG(INFO) << "Reconnecting to signaling srever...";
}

void WorkerSession::onSignalingConnected() {
    LOG(INFO) << "Connected to signaling server";
    auto msg = std::make_shared<ltproto::signaling::JoinRoom>();
    msg->set_session_id(service_id_);
    msg->set_room_id(room_id_);
    sendToSignalingServer(ltproto::id(msg), msg);

    // 当前线程模型没有cancel功能，需要搞一个flag
    if (!signaling_keepalive_inited_) {
        signaling_keepalive_inited_ = true;
        sendKeepAliveToSignalingServer();
    }
}

void WorkerSession::onSignalingJoinRoomAck(std::shared_ptr<google::protobuf::MessageLite> _msg) {
    auto msg = std::static_pointer_cast<ltproto::signaling::JoinRoomAck>(_msg);
    if (msg->err_code() != ltproto::ErrorCode::Success) {
        LOG(ERR) << "Join signaling room failed, room:" << room_id_;
        join_signaling_room_success_ = false;
    }
    else {
        join_signaling_room_success_ = true;
    }
    maybeOnCreateSessionCompleted();
}

void WorkerSession::onSignalingMessage(std::shared_ptr<google::protobuf::MessageLite> _msg) {
    auto msg = std::static_pointer_cast<ltproto::signaling::SignalingMessage>(_msg);
    switch (msg->level()) {
    case ltproto::signaling::SignalingMessage::Core:
        dispatchSignalingMessageCore(_msg);
        break;
    case ltproto::signaling::SignalingMessage::Rtc:
        dispatchSignalingMessageRtc(_msg);
        break;
    default:
        LOG(ERR) << "Unknown signaling message level " << msg->level();
        break;
    }
}

void WorkerSession::onSignalingMessageAck(std::shared_ptr<google::protobuf::MessageLite> _msg) {
    auto msg = std::static_pointer_cast<ltproto::signaling::SignalingMessageAck>(_msg);
    switch (msg->err_code()) {
    case ltproto::ErrorCode::Success:
        // do nothing
        break;
    case ltproto::ErrorCode::SignalingPeerNotOnline:
        LOG(ERR) << "Send signaling message failed, remote device not online";
        break;
    default:
        LOG(ERR) << "Send signaling message failed";
        break;
    }
}

void WorkerSession::dispatchSignalingMessageRtc(
    std::shared_ptr<google::protobuf::MessageLite> _msg) {
    auto msg = std::static_pointer_cast<ltproto::signaling::SignalingMessage>(_msg);
    LOG(DEBUG) << "Received signaling key:" << msg->rtc_message().key().c_str()
               << ", value:" << msg->rtc_message().value().c_str();
    tp_server_->onSignalingMessage(msg->rtc_message().key().c_str(),
                                   msg->rtc_message().value().c_str());
}

void WorkerSession::dispatchSignalingMessageCore(
    std::shared_ptr<google::protobuf::MessageLite> _msg) {
    auto msg = std::static_pointer_cast<ltproto::signaling::SignalingMessage>(_msg);
    auto& coremsg = msg->core_message();
    LOG(DEBUG) << "Dispatch signaling core message: " << coremsg.key();
    if (coremsg.key() == kSigCoreClose) {
        onClosed(CloseReason::ClientClose);
    }
}

void WorkerSession::sendSigClose() {
    auto msg = std::make_shared<ltproto::signaling::SignalingMessage>();
    auto coremsg = msg->mutable_core_message();
    coremsg->set_key(kSigCoreClose);
    sendToSignalingServer(ltproto::id(msg), msg);
}

void WorkerSession::sendToSignalingServer(uint32_t type,
                                          std::shared_ptr<google::protobuf::MessageLite> msg) {
    signaling_client_->send(type, msg);
}

void WorkerSession::sendKeepAliveToSignalingServer() {
    auto msg = std::make_shared<ltproto::common::KeepAlive>();
    sendToSignalingServer(ltproto::id(msg), msg);
    // 10秒发一个心跳包，当前服务端不会检测超时
    // 但是反向代理比如nginx可能设置了proxy_timeout，超过这个时间没有包就会被断链
    postDelayTask(10'000, std::bind(&WorkerSession::sendKeepAliveToSignalingServer, this));
}

bool WorkerSession::initPipeServer(ltlib::IOLoop* ioloop) {
    ltlib::Server::Params params{};
    params.stype = ltlib::StreamType::Pipe;
    params.ioloop = ioloop;
    params.pipe_name = "\\\\?\\pipe\\" + pipe_name_;
    params.on_accepted = std::bind(&WorkerSession::onPipeAccepted, this, std::placeholders::_1);
    params.on_closed = std::bind(&WorkerSession::onPipeDisconnected, this, std::placeholders::_1);
    params.on_message = std::bind(&WorkerSession::onPipeMessage, this, std::placeholders::_1,
                                  std::placeholders::_2, std::placeholders::_3);
    pipe_server_ = ltlib::Server::create(params);
    if (pipe_server_ == nullptr) {
        LOG(ERR) << "Init pipe server failed";
        return false;
    }
    return true;
}

void WorkerSession::onPipeAccepted(uint32_t fd) {
    if (pipe_client_fd_ != std::numeric_limits<uint32_t>::max()) {
        LOG(WARNING) << "New worker(" << fd << ") connected to service, but another worker(" << fd
                     << ") already being serve";
        pipe_server_->close(fd);
        return;
    }
    pipe_client_fd_ = fd;
    LOG(INFO) << "Pipe server accpeted worker(" << fd << ")";
}

void WorkerSession::onPipeDisconnected(uint32_t fd) {
    if (pipe_client_fd_ != fd) {
        LOG(FATAL) << "Worker(" << fd << ") disconnected, but we are serving worker("
                   << pipe_client_fd_ << ")";
        return;
    }
    pipe_client_fd_ = std::numeric_limits<uint32_t>::max();
    LOGF(INFO, "Worker(%u) disconnected from pipe server", fd);
}

void WorkerSession::onPipeMessage(uint32_t fd, uint32_t type,
                                  std::shared_ptr<google::protobuf::MessageLite> msg) {
    LOGF(DEBUG, "Received pipe message {fd:%u, type:%u}", fd, type);
    if (fd != pipe_client_fd_) {
        LOG(FATAL) << "fd != pipe_client_fd_";
        return;
    }
    namespace ltype = ltproto::type;
    switch (type) {
    case ltype::kKeepAliveAck:
        onKeepAliveAck();
        break;
    case ltype::kStartWorkingAck:
        onStartWorkingAck(msg);
        break;
    case ltype::kVideoFrame:
        onCapturedVideo(msg);
        break;
    case ltype::kStreamingParams:
        onWorkerStreamingParams(msg);
        break;
    case ltype::kAudioData:
        onCapturedAudio(msg);
        break;
    case ltype::kChangeStreamingParams:
        onChangeStreamingParams(msg);
        [[fallthrough]];
    case ltype::kCursorInfo:
        bypassToClient(type, msg);
        break;
    default:
        LOG(WARNING) << "Unknown message type:" << type;
        break;
    }
}

void WorkerSession::startWorking() {
    // NOTE: 这是运行在transport的线程
    auto msg = std::make_shared<ltproto::worker2service::StartWorking>();
    sendToWorkerFromOtherThread(ltproto::id(msg), msg);
}

void WorkerSession::onStartWorkingAck(std::shared_ptr<google::protobuf::MessageLite> _msg) {
    auto msg = std::static_pointer_cast<ltproto::worker2service::StartWorkingAck>(_msg);
    if (!first_start_working_ack_received_) {
        first_start_working_ack_received_ = true;
        auto ack = std::make_shared<ltproto::client2worker::StartTransmissionAck>();
        if (msg->err_code() == ltproto::ErrorCode::Success) {
            ack->set_err_code(ltproto::ErrorCode::Success);
            for (uint32_t type : msg->msg_type()) {
                worker_registered_msg_.insert(type);
            }
        }
        else {
            // TODO: 失败了，关闭整个WorkerSession
            ack->set_err_code(msg->err_code());
        }
        sendMessageToRemoteClient(ltproto::id(ack), ack, true);
        tellAppAccpetedConnection();
        postDelayTask(
            1000, std::bind(&WorkerSession::sendConnectionStatus, this, true, false, false, false));
    }
    else {
        if (msg->err_code() != ltproto::ErrorCode::Success) {
            LOG(ERR) << "Received StartWorkingAck with error code "
                     << static_cast<int>(msg->err_code()) << " : "
                     << ltproto::ErrorCode_Name(msg->err_code());
            onClosed(CloseReason::WorkerFailed);
        }
    }
}

void WorkerSession::sendToWorker(uint32_t type,
                                 std::shared_ptr<google::protobuf::MessageLite> msg) {
    pipe_server_->send(pipe_client_fd_, type, msg);
}

void WorkerSession::sendToWorkerFromOtherThread(
    uint32_t type, std::shared_ptr<google::protobuf::MessageLite> msg) {
    postTask([this, type, msg]() { sendToWorker(type, msg); });
}

void WorkerSession::onKeepAliveAck() {
    auto ack = std::make_shared<ltproto::common::KeepAliveAck>();
    sendMessageToRemoteClient(ltproto::id(ack), ack, true);
}

void WorkerSession::onWorkerStreamingParams(std::shared_ptr<google::protobuf::MessageLite> msg) {
    if (negotiated_streaming_params_ == nullptr) {
        // 第一次收到Worker进程的onWorkerStreamingParams
        negotiated_streaming_params_ = msg;
        if (worker_process_) {
            auto msg2 = std::static_pointer_cast<ltproto::common::StreamingParams>(msg);
            worker_process_->changeResolution(msg2->video_width(), msg2->video_height());
        }
        maybeOnCreateSessionCompleted();
    }
    else {
        auto start_working = std::make_shared<ltproto::worker2service::StartWorking>();
        sendToWorker(ltproto::id(start_working), start_working);
    }
}

void WorkerSession::onWorkerFailedFromOtherThread() {
    postTask([this]() { onClosed(CloseReason::WorkerFailed); });
}

void WorkerSession::onTpData(void* user_data, const uint8_t* data, uint32_t size, bool reliable) {
    // 跑在数据通道线程
    auto that = reinterpret_cast<WorkerSession*>(user_data);
    (void)reliable;
    auto type = reinterpret_cast<const uint32_t*>(data);
    // 来自client，发给server，的消息.
    auto msg = ltproto::create_by_type(*type);
    if (msg == nullptr) {
        LOG(ERR) << "Unknown message type: " << *type;
    }
    bool success = msg->ParseFromArray(data + 4, size - 4);
    if (!success) {
        LOG(ERR) << "Parse message failed, type: " << *type;
        return;
    }
    that->dispatchDcMessage(*type, msg);
}

void WorkerSession::onTpAccepted(void* user_data, lt::LinkType link_type) {
    // 跑在数据通道线程
    auto that = reinterpret_cast<WorkerSession*>(user_data);
    that->postTask([that, link_type]() {
        LOG(INFO) << "Accepted client";
        that->is_p2p_ = link_type != lt::LinkType::RelayUDP;
        that->updateLastRecvTime();
        that->syncTime();
        that->postTask(std::bind(&WorkerSession::checkKeepAliveTimeout, that));
    });
}

void WorkerSession::onTpConnChanged(void*) {}

void WorkerSession::onTpFailed(void* user_data) {
    auto that = reinterpret_cast<WorkerSession*>(user_data);
    that->postTask(std::bind(&WorkerSession::onClosed, that, CloseReason::Timeout));
}

void WorkerSession::onTpDisconnected(void* user_data) {
    auto that = reinterpret_cast<WorkerSession*>(user_data);
    that->postTask(std::bind(&WorkerSession::onClosed, that, CloseReason::Timeout));
}

void WorkerSession::onTpSignalingMessage(void* user_data, const char* key, const char* value) {
    auto that = reinterpret_cast<WorkerSession*>(user_data);
    auto signaling_msg = std::make_shared<ltproto::signaling::SignalingMessage>();
    signaling_msg->set_level(ltproto::signaling::SignalingMessage::Rtc);
    auto rtc_msg = signaling_msg->mutable_rtc_message();
    rtc_msg->set_key(key);
    rtc_msg->set_value(value);
    std::string str = signaling_msg->SerializeAsString();
    if (str.empty()) {
        LOG(ERR) << "Serialize signaling rtc message failed";
        return;
    }
    that->postTask([that, signaling_msg]() {
        that->sendToSignalingServer(ltproto::id(signaling_msg), signaling_msg);
    });
}

void WorkerSession::onTpRequestKeyframe(void* user_data) {
    auto that = reinterpret_cast<WorkerSession*>(user_data);
    auto msg = std::make_shared<ltproto::client2worker::RequestKeyframe>();
    that->sendToWorkerFromOtherThread(ltproto::id(msg), msg);
}

void WorkerSession::onTpLossRateUpdate(void* user_data, float rate) {
    auto that = reinterpret_cast<WorkerSession*>(user_data);
    that->postTask([that, rate]() {
        that->loss_rate_ = rate;
        LOG(DEBUG) << "loss rate " << rate;
    });
}

void WorkerSession::onTpEesimatedVideoBitreateUpdate(void* user_data, uint32_t bps) {
    auto that = reinterpret_cast<WorkerSession*>(user_data);
    auto msg = std::make_shared<ltproto::worker2service::ReconfigureVideoEncoder>();
    msg->set_bitrate_bps(bps);
    that->sendToWorkerFromOtherThread(ltproto::id(msg), msg);
}

void WorkerSession::onTpStat(void* user_data, uint32_t bwe_bps, uint32_t nack) {
    auto that = reinterpret_cast<WorkerSession*>(user_data);
    that->bwe_bps_ = bwe_bps;
    auto msg = std::make_shared<ltproto::client2worker::SendSideStat>();
    msg->set_bwe(bwe_bps);
    msg->set_nack(nack);
    msg->set_loss_rate(that->loss_rate_);
    LOG(DEBUG) << "BWE " << bwe_bps << " NACK " << nack;
    that->postTask([that, msg]() { that->sendMessageToRemoteClient(ltproto::id(msg), msg, true); });
}

void WorkerSession::onCapturedVideo(std::shared_ptr<google::protobuf::MessageLite> _msg) {
    // NOTE: 这是在IOLoop线程
    if (!client_connected_) {
        return;
    }
    auto encoded_frame = std::static_pointer_cast<ltproto::client2worker::VideoFrame>(_msg);
    LOGF(DEBUG, "capture:%lld, start_enc:%lld, end_enc:%lld", encoded_frame->capture_timestamp_us(),
         encoded_frame->start_encode_timestamp_us(), encoded_frame->end_encode_timestamp_us());
    lt::VideoFrame video_frame{};
    video_frame.capture_timestamp_us = encoded_frame->capture_timestamp_us();
    video_frame.start_encode_timestamp_us = encoded_frame->start_encode_timestamp_us();
    video_frame.end_encode_timestamp_us = encoded_frame->end_encode_timestamp_us();
    video_frame.width = encoded_frame->width();
    video_frame.height = encoded_frame->height();
    video_frame.is_keyframe = encoded_frame->is_keyframe();
    video_frame.data = reinterpret_cast<const uint8_t*>(encoded_frame->frame().data());
    video_frame.size = static_cast<uint32_t>(encoded_frame->frame().size());
    video_frame.ltframe_id = encoded_frame->picture_id();
    tp_server_->sendVideo(video_frame);

    calcVideoSpeed(video_frame.size);
    // static std::ofstream out{"./service_stream", std::ios::binary};
    // out.write(reinterpret_cast<const char*>(encoded_frame.data), encoded_frame.size);
    // out.flush();
}

void WorkerSession::onCapturedAudio(std::shared_ptr<google::protobuf::MessageLite> _msg) {
    if (!client_connected_) {
        return;
    }
    auto captured_audio = std::static_pointer_cast<ltproto::client2worker::AudioData>(_msg);
    lt::AudioData audio_data{};
    audio_data.data = reinterpret_cast<const uint8_t*>(captured_audio->data().c_str());
    audio_data.size = static_cast<uint32_t>(captured_audio->data().size());
    tp_server_->sendAudio(audio_data);
}

void WorkerSession::onTimeSync(std::shared_ptr<google::protobuf::MessageLite> _msg) {
    auto msg = std::static_pointer_cast<ltproto::client2service::TimeSync>(_msg);
    auto result = time_sync_.calc(msg->t0(), msg->t1(), msg->t2(), ltlib::steady_now_us());
    if (result.has_value()) {
        rtt_ = result->rtt;
        time_diff_ = result->time_diff;
        LOG(DEBUG) << "rtt:" << rtt_ << ", time_diff:" << time_diff_;
    }
}

void WorkerSession::dispatchDcMessage(uint32_t type,
                                      const std::shared_ptr<google::protobuf::MessageLite>& msg) {
    updateLastRecvTime();
    namespace ltype = ltproto::type;
    switch (type) {
    case ltype::kKeepAlive:
        onKeepAlive(msg);
        return;
    case ltype::kStartTransmission:
        onStartTransmission(msg);
        return;
    case ltype::kTimeSync:
        onTimeSync(msg);
        return;
    case ltype::kMouseEvent:
    {
        auto mouse_msg = std::static_pointer_cast<ltproto::client2worker::MouseEvent>(msg);
        if (mouse_msg->has_key_falg()) {
            postTask(
                std::bind(&WorkerSession::sendConnectionStatus, this, false, false, false, true));
        }
        if (!enable_mouse_) {
            return;
        }
        break;
    }
    case ltype::kTouchEvent:
    {
        postTask(std::bind(&WorkerSession::sendConnectionStatus, this, false, false, false, true));
        if (!enable_mouse_) {
            return;
        }
        break;
    }
    case ltype::kKeyboardEvent:
        postTask(std::bind(&WorkerSession::sendConnectionStatus, this, false, false, true, false));
        if (!enable_keyboard_) {
            return;
        }
        break;
    case ltype::kControllerStatus:
        postTask(std::bind(&WorkerSession::sendConnectionStatus, this, false, true, false, false));
        if (!enable_gamepad_) {
            return;
        }
        break;
    default:
        break;
    }
    if (worker_registered_msg_.find(type) != worker_registered_msg_.cend()) {
        sendToWorkerFromOtherThread(type, msg);
    }
}

void WorkerSession::onStartTransmission(std::shared_ptr<google::protobuf::MessageLite> _msg) {
    auto ack = std::make_shared<ltproto::client2worker::StartTransmissionAck>();
    if (client_connected_) {
        ack->set_err_code(ltproto::ErrorCode::Success);
        sendMessageToRemoteClient(ltproto::id(ack), ack, true);
        return;
    }
    client_connected_ = true;
    auto msg = std::static_pointer_cast<ltproto::client2worker::StartTransmission>(_msg);
    if (msg->token() != auth_token_) {
        LOG(ERR) << "Received SetupConnection with invalid token: " << msg->token();
        ack->set_err_code(ltproto::ErrorCode::AuthFailed);
        sendMessageToRemoteClient(ltproto::id(ack), ack, true);
        return;
    }
    startWorking();
    // 暂时不回Ack，等到worker process回了StartWorkingAck再回.
}

void WorkerSession::onKeepAlive(std::shared_ptr<google::protobuf::MessageLite> msg) {
    // 是否需给client要回ack
    // 转发给worker
    postTask([this, msg]() { sendToWorker(ltproto::type::kKeepAlive, msg); });
}

void WorkerSession::updateLastRecvTime() {
    last_recv_time_us_ = ltlib::steady_now_us();
}

void WorkerSession::checkKeepAliveTimeout() {
    // NOTE: 运行在IOLOOP
    constexpr auto kTimeoutMS = 5000;
    constexpr auto kTimeoutUS = kTimeoutMS * 1000;
    auto now = ltlib::steady_now_us();
    if (now - last_recv_time_us_ > kTimeoutUS) {
        tp_server_->close();
        onClosed(CloseReason::Timeout);
    }
    else {
        postDelayTask(kTimeoutMS, std::bind(&WorkerSession::checkKeepAliveTimeout, this));
    }
}

void WorkerSession::checkAcceptTimeout() {
    if (last_recv_time_us_ == 0) {
        onClosed(CloseReason::Timeout);
    }
}

void WorkerSession::syncTime() {
    auto msg = std::make_shared<ltproto::client2service::TimeSync>();
    msg->set_t0(time_sync_.getT0());
    msg->set_t1(time_sync_.getT1());
    msg->set_t2(ltlib::steady_now_us());
    sendMessageToRemoteClient(ltproto::id(msg), msg, true);
    constexpr uint32_t k500ms = 500;
    postDelayTask(k500ms, std::bind(&WorkerSession::syncTime, this));
}

void WorkerSession::tellAppAccpetedConnection() {
    auto msg = std::make_shared<ltproto::service2app::AcceptedConnection>();
    msg->set_device_id(client_device_id_);
    msg->set_enable_gamepad(enable_gamepad_);
    msg->set_enable_keyboard(enable_keyboard_);
    msg->set_enable_mouse(enable_mouse_);
    msg->set_gpu_decode(true); // 当前只支持硬编硬解
    msg->set_gpu_encode(true);
    msg->set_p2p(is_p2p_);
    auto negotiated_params =
        std::static_pointer_cast<ltproto::common::StreamingParams>(negotiated_streaming_params_);
    msg->set_video_codec((ltproto::common::VideoCodecType)negotiated_params->video_codecs().Get(0));
    on_accepted_connection_(msg);
}

void WorkerSession::sendConnectionStatus(bool repeat, bool gp_hit, bool kb_hit, bool mouse_hit) {
    auto status = std::make_shared<ltproto::service2app::ConnectionStatus>();
    // FIXME: 这个值是错的，我们要显示实际值，而这里是估计值
    status->set_bandwidth_bps(static_cast<int32_t>(video_send_bps_));
    status->set_delay_ms(static_cast<int32_t>(rtt_ / 2 / 1000));
    status->set_device_id(client_device_id_);
    status->set_enable_gamepad(enable_gamepad_);
    status->set_enable_keyboard(enable_keyboard_);
    status->set_enable_mouse(enable_mouse_);
    status->set_hit_gamepad(gp_hit);
    status->set_hit_keyboard(kb_hit);
    status->set_hit_mouse(mouse_hit);
    status->set_p2p(is_p2p_);
    on_connection_status_(status);
    if (repeat) {
        postDelayTask(
            1000, std::bind(&WorkerSession::sendConnectionStatus, this, true, false, false, false));
    }
}

void WorkerSession::calcVideoSpeed(int64_t new_frame_bytes) {
    SpeedEntry se{};
    auto now_ms = ltlib::steady_now_ms();
    se.timestamp_ms = now_ms;
    se.value = new_frame_bytes;
    video_send_history_.push_back(se);
    while (!video_send_history_.empty()) {
        if (video_send_history_.front().timestamp_ms + 1000 < now_ms) {
            video_send_history_.pop_front();
        }
        else {
            break;
        }
    }
    int64_t sum = 0;
    for (auto& entry : video_send_history_) {
        sum += entry.value;
    }
    video_send_bps_ = sum * 8;
}

bool WorkerSession::sendMessageToRemoteClient(
    uint32_t type, const std::shared_ptr<google::protobuf::MessageLite>& msg, bool reliable) {
    if (!client_connected_) {
        return false;
    }
    auto packet = ltproto::Packet::create({type, msg}, false);
    if (!packet.has_value()) {
        LOG(ERR) << "Create Peer2Peer packet failed, type:" << type;
        return false;
    }
    const auto& pkt = packet.value();
    // rtc的数据通道可以帮助我们完成stream->packet的过程，所以这里不需要把packet header一起传过去.
    bool success = tp_server_->sendData(pkt.payload.get(), pkt.header.payload_size, reliable);
    return success;
}

void WorkerSession::bypassToClient(uint32_t type,
                                   std::shared_ptr<google::protobuf::MessageLite> msg) {
    sendMessageToRemoteClient(type, msg, true);
}

void WorkerSession::onChangeStreamingParams(std::shared_ptr<google::protobuf::MessageLite> _msg) {
    auto msg = std::static_pointer_cast<ltproto::client2worker::ChangeStreamingParams>(_msg);
    auto width = static_cast<uint32_t>(msg->params().video_width());
    auto height = static_cast<uint32_t>(msg->params().video_height());
    if (worker_process_) {
        worker_process_->changeResolution(width, height);
    }
    else {
        LOG(ERR) << "Received ChangeStreamingParams but worker_process_ == nullptr";
    }
}

} // namespace svc

} // namespace lt