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

#include <ltproto/peer2peer/audio_data.pb.h>
#include <ltproto/peer2peer/keep_alive.pb.h>
#include <ltproto/peer2peer/reconfigure_video_encoder.pb.h>
#include <ltproto/peer2peer/request_keyframe.pb.h>
#include <ltproto/peer2peer/send_side_stat.pb.h>
#include <ltproto/peer2peer/start_transmission.pb.h>
#include <ltproto/peer2peer/start_transmission_ack.pb.h>
#include <ltproto/peer2peer/start_working.pb.h>
#include <ltproto/peer2peer/start_working_ack.pb.h>
#include <ltproto/peer2peer/stop_working.pb.h>
#include <ltproto/peer2peer/time_sync.pb.h>
#include <ltproto/peer2peer/video_frame.pb.h>
#include <ltproto/server/open_connection.pb.h>
#include <ltproto/signaling/join_room.pb.h>
#include <ltproto/signaling/join_room_ack.pb.h>
#include <ltproto/signaling/signaling_message.pb.h>
#include <ltproto/signaling/signaling_message_ack.pb.h>

#include <ltlib/system.h>
#include <ltlib/times.h>

#include <transport/transport_rtc.h>
#include <transport/transport_rtc2.h>
#include <transport/transport_tcp.h>

#include "worker_process.h"
#include <string_keys.h>

namespace {

lt::VideoCodecType to_ltrtc(ltproto::peer2peer::VideoCodecType type) {
    switch (type) {
    case ltproto::peer2peer::VideoCodecType::AVC:
        return lt::VideoCodecType::H264;
    case ltproto::peer2peer::VideoCodecType::HEVC:
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
    std::shared_ptr<WorkerSession> session{
        new WorkerSession(params.name, params.user_defined_relay_server, params.on_create_completed,
                          params.on_closed)};
    if (!session->init(params.msg)) {
        return nullptr;
    }
    return session;
}

WorkerSession::WorkerSession(
    const std::string& name, const std::string& relay_server,
    std::function<void(bool, const std::string&, std::shared_ptr<google::protobuf::MessageLite>)>
        on_create_completed,
    std::function<void(CloseReason, const std::string&, const std::string&)> on_closed)
    : session_name_(name)
    , user_defined_relay_server_(relay_server)
    , on_create_session_completed_(on_create_completed)
    , on_closed_(on_closed) {
    constexpr int kRandLength = 4;
    // 是否需要global?
    pipe_name_ = "Lanthing_worker_";
    for (int i = 0; i < kRandLength; ++i) {
        pipe_name_.push_back(rand() % 26 + 'A');
    }
}

WorkerSession::~WorkerSession() {
    {
        std::lock_guard lock{mutex_};
        signaling_client_.reset();
        pipe_server_.reset();
        ioloop_.reset();
    }
}

bool WorkerSession::init(std::shared_ptr<google::protobuf::MessageLite> _msg) {
    auto msg = std::static_pointer_cast<ltproto::server::OpenConnection>(_msg);
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
        switch (ltproto::peer2peer::VideoCodecType(codec)) {
        case ltproto::peer2peer::VideoCodecType::AVC:
            client_codecs.push_back(lt::VideoCodecType::H264);
            break;
        case ltproto::peer2peer::VideoCodecType::HEVC:
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

    ioloop_ = ltlib::IOLoop::create();
    if (ioloop_ == nullptr) {
        LOG(WARNING) << "Init IOLoop failed";
        return false;
    }
    if (!initSignlingClient()) {
        LOG(WARNING) << "Init signaling client failed";
        return false;
    }
    if (!initPipeServer()) {
        LOG(WARNING) << "Init worker pipe server failed";
        return false;
    }
    createWorkerProcess((uint32_t)client_width, (uint32_t)client_height,
                        (uint32_t)client_refresh_rate, client_codecs);
    std::promise<void> promise;
    auto future = promise.get_future();
    thread_ = ltlib::BlockingThread::create(
        "worker_session", [this, &promise](const std::function<void()>& i_am_alive) {
            promise.set_value();
            mainLoop(i_am_alive);
        });
    future.get();
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
    else {
        rtc_closed_ = false;
        return true;
    }
}

std::unique_ptr<tp::Server> WorkerSession::createTcpServer() {
    namespace ph = std::placeholders;
    auto negotiated_params =
        std::static_pointer_cast<ltproto::peer2peer::StreamingParams>(negotiated_streaming_params_);
    lt::tp::ServerTCP::Params params{};
    params.video_codec_type = ::to_ltrtc(
        static_cast<ltproto::peer2peer::VideoCodecType>(negotiated_params->video_codecs().Get(0)));
    params.on_failed = std::bind(&WorkerSession::onTpFailed, this);
    params.on_disconnected = std::bind(&WorkerSession::onTpDisconnected, this);
    params.on_accepted = std::bind(&WorkerSession::onTpAccepted, this);
    params.on_data = std::bind(&WorkerSession::onTpData, this, ph::_1, ph::_2, ph::_3);
    params.on_signaling_message =
        std::bind(&WorkerSession::onTpSignalingMessage, this, ph::_1, ph::_2);
    return lt::tp::ServerTCP::create(params);
}

std::unique_ptr<tp::Server> WorkerSession::createRtcServer() {
    namespace ph = std::placeholders;
    auto negotiated_params =
        std::static_pointer_cast<ltproto::peer2peer::StreamingParams>(negotiated_streaming_params_);

    rtc::Server::Params params{};
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
        params.nbp2p_params.disable_ipv6 = false;
        params.nbp2p_params.disable_lan_udp = false;
        params.nbp2p_params.disable_mapping = false;
        params.nbp2p_params.disable_reflex = false;
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
        static_cast<ltproto::peer2peer::VideoCodecType>(negotiated_params->video_codecs().Get(0)));
    params.on_failed = std::bind(&WorkerSession::onTpFailed, this);
    params.on_disconnected = std::bind(&WorkerSession::onTpDisconnected, this);
    params.on_accepted = std::bind(&WorkerSession::onTpAccepted, this);
    params.on_conn_changed = std::bind(&WorkerSession::onTpConnChanged, this);
    params.on_data = std::bind(&WorkerSession::onTpData, this, ph::_1, ph::_2, ph::_3);
    params.on_signaling_message =
        std::bind(&WorkerSession::onTpSignalingMessage, this, ph::_1, ph::_2);
    params.on_keyframe_request = std::bind(&WorkerSession::onTpRequestKeyframe, this);
    params.on_video_bitrate_update =
        std::bind(&WorkerSession::onTpEesimatedVideoBitreateUpdate, this, ph::_1);
    params.on_loss_rate_update = std::bind(&WorkerSession::onTpLossRateUpdate, this, ph::_1);
    return rtc::Server::create(std::move(params));
}

std::unique_ptr<tp::Server> WorkerSession::createRtc2Server() {
    namespace ph = std::placeholders;
    rtc2::Server::Params params{};
    params.on_accepted = std::bind(&WorkerSession::onTpAccepted, this);
    params.on_failed = std::bind(&WorkerSession::onTpFailed, this);
    params.on_disconnected = std::bind(&WorkerSession::onTpDisconnected, this);
    params.on_conn_changed = std::bind(&WorkerSession::onTpConnChanged, this);
    params.on_data = std::bind(&WorkerSession::onTpData, this, ph::_1, ph::_2, ph::_3);
    params.on_signaling_message =
        std::bind(&WorkerSession::onTpSignalingMessage, this, ph::_1, ph::_2);
    params.on_keyframe_request = std::bind(&WorkerSession::onTpRequestKeyframe, this);
    params.on_video_bitrate_update =
        std::bind(&WorkerSession::onTpEesimatedVideoBitreateUpdate, this, ph::_1);
    params.on_loss_rate_update = std::bind(&WorkerSession::onTpLossRateUpdate, this, ph::_1);
    params.remote_digest;
    params.key_and_cert = rtc2::KeyAndCert::create();
    if (params.key_and_cert == nullptr) {
        return nullptr;
    }
    params.video_send_ssrc = 541651314;
    params.audio_send_ssrc = 687154681;
    return rtc2::Server::create(params);
}

void WorkerSession::mainLoop(const std::function<void()>& i_am_alive) {
    LOG(INFO) << "Worker session enter main loop";
    ioloop_->run(i_am_alive);
    LOG(INFO) << "Worker session exit main loop";
}

void WorkerSession::createWorkerProcess(uint32_t client_width, uint32_t client_height,
                                        uint32_t client_refresh_rate,
                                        std::vector<lt::VideoCodecType> client_codecs) {
    WorkerProcess::Params params{};
    params.pipe_name = pipe_name_;
    params.on_stoped = std::bind(&WorkerSession::onWorkerStoped, this);
    params.path = ltlib::getProgramFullpath<char>();
    params.client_width = client_width;
    params.client_height = client_height;
    params.client_refresh_rate = client_refresh_rate;
    params.client_codecs = client_codecs;
    worker_process_ = WorkerProcess::create(params);
    worker_process_stoped_ = false;
}

void WorkerSession::onClosed(CloseReason reason) {
    // NOTE: 运行在ioloop
    switch (reason) {
    case CloseReason::ClientClose:
        rtc_closed_ = true;
        break;
    case CloseReason::HostClose:
        worker_process_stoped_ = true;
        break;
    case CloseReason::TimeoutClose:
        rtc_closed_ = true;
        break;
    default:
        break;
    }
    if (!rtc_closed_) {
        tp_server_->close();
        rtc_closed_ = true;
    }
    if (!worker_process_stoped_) {
        auto msg = std::make_shared<ltproto::peer2peer::StopWorking>();
        sendToWorker(ltproto::id(msg), msg);
    }
    if (rtc_closed_ && worker_process_stoped_) {
        on_closed_(reason, session_name_, room_id_);
    }
}

void WorkerSession::maybeOnCreateSessionCompleted() {
    auto empty_params = std::make_shared<ltproto::peer2peer::StreamingParams>();
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
    std::lock_guard lock{mutex_};
    if (ioloop_) {
        ioloop_->post(task);
    }
}

void WorkerSession::postDelayTask(int64_t delay_ms, const std::function<void()>& task) {
    std::lock_guard lock{mutex_};
    if (ioloop_) {
        ioloop_->postDelay(delay_ms, task);
    }
}

bool WorkerSession::initSignlingClient() {
    ltlib::Client::Params params{};
    params.stype = ltlib::StreamType::TCP;
    params.ioloop = ioloop_.get();
    params.host = signaling_addr_;
    params.port = signaling_port_;
    params.is_tls = false;
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

void WorkerSession::onSignalingMessageFromNet(uint32_t type,
                                              std::shared_ptr<google::protobuf::MessageLite> msg) {
    namespace ltype = ltproto::type;
    switch (type) {
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
    auto msg = std::make_shared<ltproto::signaling::JoinRoom>();
    msg->set_session_id(service_id_);
    msg->set_room_id(room_id_);
    signaling_client_->send(ltproto::id(msg), msg);
}

void WorkerSession::onSignalingJoinRoomAck(std::shared_ptr<google::protobuf::MessageLite> _msg) {
    auto msg = std::static_pointer_cast<ltproto::signaling::JoinRoomAck>(_msg);
    if (msg->err_code() != ltproto::signaling::JoinRoomAck_ErrCode_Success) {
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
    case ltproto::signaling::SignalingMessageAck_ErrCode_Success:
        // do nothing
        break;
    case ltproto::signaling::SignalingMessageAck_ErrCode_NotOnline:
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
    LOG(INFO) << "Received signaling key:" << msg->rtc_message().key().c_str()
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

bool WorkerSession::initPipeServer() {
    ltlib::Server::Params params{};
    params.stype = ltlib::StreamType::Pipe;
    params.ioloop = ioloop_.get();
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
    LOGF(INFO, "Worker(%d) disconnected from pipe server", fd);
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
    default:
        LOG(WARNING) << "Unknown message type:" << type;
        break;
    }
}

void WorkerSession::startWorking() {
    // NOTE: 这是运行在transport的线程
    auto msg = std::make_shared<ltproto::peer2peer::StartWorking>();
    sendToWorkerFromOtherThread(ltproto::id(msg), msg);
}

void WorkerSession::onStartWorkingAck(std::shared_ptr<google::protobuf::MessageLite> _msg) {
    auto msg = std::static_pointer_cast<ltproto::peer2peer::StartWorkingAck>(_msg);
    auto ack = std::make_shared<ltproto::peer2peer::StartTransmissionAck>();
    if (msg->err_code() == ltproto::peer2peer::StartWorkingAck_ErrCode_Success) {
        ack->set_err_code(ltproto::peer2peer::StartTransmissionAck_ErrCode_Success);
        for (uint32_t type : msg->msg_type()) {
            worker_registered_msg_.insert(type);
        }
    }
    else {
        // TODO: 失败了，关闭整个WorkerSession
        ack->set_err_code(ltproto::peer2peer::StartTransmissionAck_ErrCode_HostFailed);
    }
    sendMessageToRemoteClient(ltproto::id(ack), ack, true);
}

void WorkerSession::sendToWorker(uint32_t type,
                                 std::shared_ptr<google::protobuf::MessageLite> msg) {
    pipe_server_->send(pipe_client_fd_, type, msg);
}

void WorkerSession::sendToWorkerFromOtherThread(
    uint32_t type, std::shared_ptr<google::protobuf::MessageLite> msg) {
    postTask([this, type, msg]() { sendToWorker(type, msg); });
}

void WorkerSession::onWorkerStoped() {
    // FIXME: worker退出不代表关闭，也可能是session改变
    // NOTE: 这是运行在WorkerProcess线程
    postTask(std::bind(&WorkerSession::onClosed, this, CloseReason::HostClose));
}

void WorkerSession::onWorkerStreamingParams(std::shared_ptr<google::protobuf::MessageLite> msg) {
    negotiated_streaming_params_ = msg;
    sendWorkerKeepAlive();
    maybeOnCreateSessionCompleted();
}

void WorkerSession::sendWorkerKeepAlive() {
    // 运行在ioloop
    auto msg = std::make_shared<ltproto::peer2peer::KeepAlive>();
    sendToWorker(ltproto::id(msg), msg);
    postDelayTask(500, std::bind(&WorkerSession::sendWorkerKeepAlive, this));
}

void WorkerSession::onTpData(const uint8_t* data, uint32_t size, bool reliable) {
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
    dispatchDcMessage(*type, msg);
}

void WorkerSession::onTpAccepted() {
    postTask([this]() {
        LOG(INFO) << "Accepted client";
        updateLastRecvTime();
        syncTime();
        postTask(std::bind(&WorkerSession::checkTimeout, this));
        postTask(std::bind(&WorkerSession::getTransportStat, this));
    });
}

void WorkerSession::onTpConnChanged() {}

void WorkerSession::onTpFailed() {
    postTask(std::bind(&WorkerSession::onClosed, this, CloseReason::TimeoutClose));
}

void WorkerSession::onTpDisconnected() {
    postTask(std::bind(&WorkerSession::onClosed, this, CloseReason::TimeoutClose));
}

void WorkerSession::onTpSignalingMessage(const std::string& key, const std::string& value) {
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
    postTask([this, signaling_msg]() {
        signaling_client_->send(ltproto::id(signaling_msg), signaling_msg);
    });
}

void WorkerSession::onTpRequestKeyframe() {
    auto msg = std::make_shared<ltproto::peer2peer::RequestKeyframe>();
    sendToWorkerFromOtherThread(ltproto::id(msg), msg);
}

void WorkerSession::onTpLossRateUpdate(float rate) {
    postTask([this, rate]() {
        loss_rate_ = rate;
        LOG(DEBUG) << "loss rate " << rate;
    });
}

void WorkerSession::onTpEesimatedVideoBitreateUpdate(uint32_t bps) {
    auto msg = std::make_shared<ltproto::peer2peer::ReconfigureVideoEncoder>();
    msg->set_bitrate_bps(bps);
    sendToWorkerFromOtherThread(ltproto::id(msg), msg);
}

void WorkerSession::onCapturedVideo(std::shared_ptr<google::protobuf::MessageLite> _msg) {
    // NOTE: 这是在IOLoop线程
    auto encoded_frame = std::static_pointer_cast<ltproto::peer2peer::VideoFrame>(_msg);
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
    // static std::ofstream out{"./service_stream", std::ios::binary};
    // out.write(reinterpret_cast<const char*>(encoded_frame.data), encoded_frame.size);
    // out.flush();
}

void WorkerSession::onCapturedAudio(std::shared_ptr<google::protobuf::MessageLite> _msg) {
    auto captured_audio = std::static_pointer_cast<ltproto::peer2peer::AudioData>(_msg);
    lt::AudioData audio_data{};
    audio_data.data = reinterpret_cast<const uint8_t*>(captured_audio->data().c_str());
    audio_data.size = static_cast<uint32_t>(captured_audio->data().size());
    tp_server_->sendAudio(audio_data);
}

void WorkerSession::onTimeSync(std::shared_ptr<google::protobuf::MessageLite> _msg) {
    auto msg = std::static_pointer_cast<ltproto::peer2peer::TimeSync>(_msg);
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
        break;
    case ltype::kStartTransmission:
        onStartTransmission(msg);
        break;
    case ltype::kTimeSync:
        onTimeSync(msg);
        break;
    default:
        if (worker_registered_msg_.find(type) != worker_registered_msg_.cend()) {
            sendToWorkerFromOtherThread(type, msg);
        }
        break;
    }
}

void WorkerSession::onStartTransmission(std::shared_ptr<google::protobuf::MessageLite> _msg) {
    auto ack = std::make_shared<ltproto::peer2peer::StartTransmissionAck>();
    if (client_connected_) {
        ack->set_err_code(ltproto::peer2peer::StartTransmissionAck_ErrCode_Success);
        sendMessageToRemoteClient(ltproto::id(ack), ack, true);
        return;
    }
    client_connected_ = true;
    auto msg = std::static_pointer_cast<ltproto::peer2peer::StartTransmission>(_msg);
    if (msg->token() != auth_token_) {
        LOG(ERR) << "Received SetupConnection with invalid token: " << msg->token();
        ack->set_err_code(ltproto::peer2peer::StartTransmissionAck_ErrCode_AuthFailed);
        sendMessageToRemoteClient(ltproto::id(ack), ack, true);
        return;
    }
    startWorking();
    // 暂时不回Ack，等到worker process回了StartWorkingAck再回.
}

void WorkerSession::onKeepAlive(std::shared_ptr<google::protobuf::MessageLite> msg) {
    // 是否需要回ack
}

void WorkerSession::updateLastRecvTime() {
    last_recv_time_us_ = ltlib::steady_now_us();
}

void WorkerSession::checkTimeout() {
    // NOTE: 运行在IOLOOP
    constexpr auto kTimeoutMS = 3000;
    constexpr auto kTimeoutUS = kTimeoutMS * 1000;
    auto now = ltlib::steady_now_us();
    if (now - last_recv_time_us_ > kTimeoutUS) {
        tp_server_->close();
        onClosed(CloseReason::TimeoutClose);
    }
    else {
        postDelayTask(kTimeoutMS, std::bind(&WorkerSession::checkTimeout, this));
    }
}

void WorkerSession::syncTime() {
    auto msg = std::make_shared<ltproto::peer2peer::TimeSync>();
    msg->set_t0(time_sync_.getT0());
    msg->set_t1(time_sync_.getT1());
    msg->set_t2(ltlib::steady_now_us());
    sendMessageToRemoteClient(ltproto::id(msg), msg, true);
    constexpr uint32_t k500ms = 500;
    postDelayTask(k500ms, std::bind(&WorkerSession::syncTime, this));
}

void WorkerSession::getTransportStat() {
#if LT_USE_LTRTC
    rtc::Server* svr = static_cast<rtc::Server*>(tp_server_.get());
    uint32_t bwe_bps = svr->bwe();
    uint32_t nack_count = svr->nack();
    auto msg = std::make_shared<ltproto::peer2peer::SendSideStat>();
    msg->set_bwe(bwe_bps);
    msg->set_nack(nack_count);
    msg->set_loss_rate(loss_rate_);
    LOG(DEBUG) << "BWE " << bwe_bps << " NACK " << nack_count;
    sendMessageToRemoteClient(ltproto::id(msg), msg, true);
    postDelayTask(1'000, std::bind(&WorkerSession::getTransportStat, this));
#endif // LT_USE_LTRTC
}

bool WorkerSession::sendMessageToRemoteClient(
    uint32_t type, const std::shared_ptr<google::protobuf::MessageLite>& msg, bool reliable) {
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

} // namespace svc

} // namespace lt