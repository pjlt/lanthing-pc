#include "worker_session.h"

#include <fstream>

#include <g3log/g3log.hpp>

#include <ltproto/peer2peer/keep_alive.pb.h>
#include <ltproto/peer2peer/start_transmission.pb.h>
#include <ltproto/peer2peer/start_transmission_ack.pb.h>
#include <ltproto/peer2peer/start_working.pb.h>
#include <ltproto/peer2peer/start_working_ack.pb.h>
#include <ltproto/peer2peer/stop_working.pb.h>
#include <ltproto/peer2peer/video_frame.pb.h>
#include <ltproto/server/open_connection.pb.h>
#include <ltproto/signaling/join_room.pb.h>
#include <ltproto/signaling/join_room_ack.pb.h>
#include <ltproto/signaling/signaling_message.pb.h>
#include <ltproto/signaling/signaling_message_ack.pb.h>

#include <ltlib/system.h>
#include <ltlib/times.h>

#include <string_keys.h>
// #include <graphics/encoder/video_encoder.h>
#include "worker_process.h"

namespace {

rtc::VideoCodecType to_ltrtc(ltproto::peer2peer::VideoCodecType type) {
    switch (type) {
    case ltproto::peer2peer::VideoCodecType::AVC:
        return rtc::VideoCodecType::H264;
    case ltproto::peer2peer::VideoCodecType::HEVC:
        return rtc::VideoCodecType::H265;
    default:
        return rtc::VideoCodecType::Unknown;
    }
}

lt::VideoEncoder::Backend to_lt(ltproto::peer2peer::StreamingParams::VideoEncodeBackend backend) {
    switch (backend) {
    case ltproto::peer2peer::StreamingParams_VideoEncodeBackend_NvEnc:
        return lt::VideoEncoder::Backend::NvEnc;
    case ltproto::peer2peer::StreamingParams_VideoEncodeBackend_IntelMediaSDK:
        return lt::VideoEncoder::Backend::IntelMediaSDK;
    case ltproto::peer2peer::StreamingParams_VideoEncodeBackend_AMF:
        return lt::VideoEncoder::Backend::Amf;
    default:
        return lt::VideoEncoder::Backend::Unknown;
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

std::shared_ptr<WorkerSession> svc::WorkerSession::create(
    const std::string& name, std::shared_ptr<google::protobuf::MessageLite> msg,
    std::function<void(bool, const std::string&, std::shared_ptr<google::protobuf::MessageLite>)>
        on_create_completed,
    std::function<void(CloseReason, const std::string&, const std::string&)> on_closed) {
    std::shared_ptr<WorkerSession> session{new WorkerSession(name, on_create_completed, on_closed)};
    if (!session->init(msg)) {
        return nullptr;
    }
    return session;
}

WorkerSession::WorkerSession(
    const std::string& name,
    std::function<void(bool, const std::string&, std::shared_ptr<google::protobuf::MessageLite>)>
        on_create_completed,
    std::function<void(CloseReason, const std::string&, const std::string&)> on_closed)
    : session_name_(name)
    , on_create_session_completed_(on_create_completed)
    , on_closed_(on_closed) {
    ::srand(static_cast<unsigned int>(::time(nullptr)));
    constexpr int kRandLength = 4;
    // 是否需要global?
    pipe_name_ = "Lanthing_worker_";
    for (int i = 0; i < kRandLength; ++i) {
        pipe_name_.push_back(rand() % 26 + 'A');
    }
}

WorkerSession::~WorkerSession() {
    signaling_client_.reset();
    pipe_server_.reset();
    ioloop_->stop();
}

bool WorkerSession::init(std::shared_ptr<google::protobuf::MessageLite> _msg) {
    auto msg = std::static_pointer_cast<ltproto::server::OpenConnection>(_msg);
    auth_token_ = msg->auth_token();
    service_id_ = msg->service_id();
    room_id_ = msg->room_id();
    p2p_username_ = msg->p2p_username();
    p2p_password_ = msg->p2p_password();
    p2p_username_ = "p2puser";
    p2p_password_ = "p2ppassword";
    signaling_addr_ = msg->signaling_addr();
    signaling_port_ = msg->signaling_port();
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
        LOG(WARNING) << "Received OpenConnection with invalid streaming params";
        return false;
    }
    std::vector<rtc::VideoCodecType> client_codecs;
    for (auto codec : msg->streaming_params().video_codecs()) {
        switch (ltproto::peer2peer::VideoCodecType(codec.codec_type())) {
        case ltproto::peer2peer::VideoCodecType::AVC:
            client_codecs.push_back(rtc::VideoCodecType::H264);
            break;
        case ltproto::peer2peer::VideoCodecType::HEVC:
            client_codecs.push_back(rtc::VideoCodecType::H265);
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
    if (!init_signling_client()) {
        LOG(WARNING) << "Init signaling client failed";
        return false;
    }
    if (!init_pipe_server()) {
        LOG(WARNING) << "Init worker pipe server failed";
        return false;
    }
    create_worker_process((uint32_t)client_width, (uint32_t)client_height,
                          (uint32_t)client_refresh_rate, client_codecs);
    // TODO: 移除这个task_thread_
    task_thread_ = ltlib::TaskThread::create("check_timeout");
    std::promise<void> promise;
    auto future = promise.get_future();
    thread_ = ltlib::BlockingThread::create(
        "worker_session",
        [this, &promise](const std::function<void()>& i_am_alive, void*) {
            promise.set_value();
            main_loop(i_am_alive);
        },
        nullptr);
    future.get();
    return true;
}

bool WorkerSession::init_rtc_server() {
    auto negotiated_params =
        std::static_pointer_cast<ltproto::peer2peer::StreamingParams>(negotiated_streaming_params_);
    namespace ph = std::placeholders;
    rtc::Server::Params cfg;
    cfg.use_nbp2p = false;
    cfg.username = p2p_username_.c_str();
    cfg.password = p2p_password_.c_str();
    cfg.video_codec_type = ::to_ltrtc(negotiated_params->video_codecs().Get(0).codec_type());
    cfg.on_failed = std::bind(&WorkerSession::on_ltrtc_failed_thread_safe, this);
    cfg.on_disconnected = std::bind(&WorkerSession::on_ltrtc_disconnected_thread_safe, this);
    cfg.on_accepted = std::bind(&WorkerSession::on_ltrtc_accepted_thread_safe, this);
    cfg.on_conn_changed = std::bind(&WorkerSession::on_ltrtc_conn_changed, this);
    cfg.on_data = std::bind(&WorkerSession::on_ltrtc_data, this, ph::_1, ph::_2, ph::_3);
    cfg.on_signaling_message =
        std::bind(&WorkerSession::on_ltrtc_signaling_message, this, ph::_1, ph::_2);
    rtc_server_ = rtc::Server::create(std::move(cfg));
    if (rtc_server_ == nullptr) {
        LOG(WARNING) << "Create rtc server failed";
        return false;
    }
    else {
        rtc_closed_ = false;
        return true;
    }
}

void WorkerSession::main_loop(const std::function<void()>& i_am_alive) {
    LOG(INFO) << "Worker session enter main loop";
    ioloop_->run(i_am_alive);
}

void WorkerSession::create_worker_process(uint32_t client_width, uint32_t client_height,
                                          uint32_t client_refresh_rate,
                                          std::vector<rtc::VideoCodecType> client_codecs) {
    WorkerProcess::Params params{};
    params.pipe_name = pipe_name_;
    params.on_stoped = std::bind(&WorkerSession::on_worker_stoped, this);
    params.path = ltlib::get_program_fullpath<char>();
    params.client_width = client_width;
    params.client_height = client_height;
    params.client_refresh_rate = client_refresh_rate;
    params.client_codecs = client_codecs;
    worker_process_ = WorkerProcess::create(params);
    worker_process_stoped_ = false;
}

void WorkerSession::on_closed(CloseReason reason) {
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
        rtc_server_->close();
        rtc_closed_ = true;
    }
    if (!worker_process_stoped_) {
        auto msg = std::make_shared<ltproto::peer2peer::StopWorking>();
        send_to_worker(ltproto::id(msg), msg);
    }
    if (rtc_closed_ && worker_process_stoped_) {
        on_closed_(reason, session_name_, room_id_);
    }
}

void WorkerSession::maybe_on_create_session_completed() {
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
    if (!create_video_encoder()) {
        on_create_session_completed_(false, session_name_, empty_params);
        return;
    }
    if (!init_rtc_server()) {
        on_create_session_completed_(false, session_name_, empty_params);
        return;
    }
    on_create_session_completed_(true, session_name_, negotiated_streaming_params_);
}

bool WorkerSession::create_video_encoder() {
    auto params =
        std::static_pointer_cast<ltproto::peer2peer::StreamingParams>(negotiated_streaming_params_);
    if (params->video_codecs_size() == 0) {
        LOG(WARNING) << "Negotiate failed, no appropriate video codec";
        return false;
    }
    auto backend = params->video_codecs().Get(0).backend();
    auto codec = params->video_codecs().Get(0).codec_type();
    LOG(INFO) << "Negotiate success, using "
              << ltproto::peer2peer::StreamingParams_VideoEncodeBackend_Name(backend) << ":"
              << ltproto::peer2peer::VideoCodecType_Name(codec);
    lt::VideoEncoder::InitParams init_params{};
    init_params.backend = to_lt(backend);
    init_params.codec_type = to_ltrtc(codec);
    init_params.bitrate_bps = 10'000'000; // TODO: 修改更合理的值，或者协商
    init_params.width = params->video_width();
    init_params.height = params->video_height();
    video_encoder_ = lt::VideoEncoder::create(init_params);
    return video_encoder_ != nullptr;
}

bool WorkerSession::init_signling_client() {
    ltlib::Client::Params params{};
    params.stype = ltlib::StreamType::TCP;
    params.ioloop = ioloop_.get();
    params.host = signaling_addr_;
    params.port = signaling_port_;
    params.is_tls = false;
    params.on_connected = std::bind(&WorkerSession::on_signaling_connected, this);
    params.on_closed = std::bind(&WorkerSession::on_signaling_disconnected, this);
    params.on_reconnecting = std::bind(&WorkerSession::on_signaling_reconnecting, this);
    params.on_message = std::bind(&WorkerSession::on_signaling_message_from_net, this,
                                  std::placeholders::_1, std::placeholders::_2);
    signaling_client_ = ltlib::Client::create(params);
    if (signaling_client_ == nullptr) {
        return false;
    }
    return true;
}

void WorkerSession::on_signaling_message_from_net(
    uint32_t type, std::shared_ptr<google::protobuf::MessageLite> msg) {
    namespace ltype = ltproto::type;
    switch (type) {
    case ltype::kJoinRoomAck:
        on_signaling_join_room_ack(msg);
        break;
    case ltype::kSignalingMessage:
        on_signaling_message(msg);
        break;
    case ltype::kSignalingMessageAck:
        on_signaling_message_ack(msg);
        break;
    default:
        LOG(WARNING) << "Unknown signaling message type " << type;
        break;
    }
}

void WorkerSession::on_signaling_disconnected() {
    LOG(INFO) << "Disconnected from signaling srever";
}

void WorkerSession::on_signaling_reconnecting() {
    LOG(INFO) << "Reconnecting to signaling srever...";
}

void WorkerSession::on_signaling_connected() {
    auto msg = std::make_shared<ltproto::signaling::JoinRoom>();
    msg->set_session_id(service_id_);
    msg->set_room_id(room_id_);
    signaling_client_->send(ltproto::id(msg), msg);
}

void WorkerSession::on_signaling_join_room_ack(
    std::shared_ptr<google::protobuf::MessageLite> _msg) {
    auto msg = std::static_pointer_cast<ltproto::signaling::JoinRoomAck>(_msg);
    if (msg->err_code() != ltproto::signaling::JoinRoomAck_ErrCode_Success) {
        LOG(WARNING) << "Join signaling room failed, room:" << room_id_;
        join_signaling_room_success_ = false;
    }
    else {
        join_signaling_room_success_ = true;
    }
    maybe_on_create_session_completed();
}

void WorkerSession::on_signaling_message(std::shared_ptr<google::protobuf::MessageLite> _msg) {
    auto msg = std::static_pointer_cast<ltproto::signaling::SignalingMessage>(_msg);
    switch (msg->level()) {
    case ltproto::signaling::SignalingMessage::Core:
        dispatch_signaling_message_core(_msg);
        break;
    case ltproto::signaling::SignalingMessage::Rtc:
        dispatch_signaling_message_rtc(_msg);
        break;
    default:
        LOG(WARNING) << "Unknown signaling message level " << msg->level();
        break;
    }
}

void WorkerSession::on_signaling_message_ack(std::shared_ptr<google::protobuf::MessageLite> _msg) {
    auto msg = std::static_pointer_cast<ltproto::signaling::SignalingMessageAck>(_msg);
    switch (msg->err_code()) {
    case ltproto::signaling::SignalingMessageAck_ErrCode_Success:
        // do nothing
        break;
    case ltproto::signaling::SignalingMessageAck_ErrCode_NotOnline:
        LOG(WARNING) << "Send signaling message failed, remote device not online";
        break;
    default:
        LOG(WARNING) << "Send signaling message failed";
        break;
    }
}

void WorkerSession::dispatch_signaling_message_rtc(
    std::shared_ptr<google::protobuf::MessageLite> _msg) {
    auto msg = std::static_pointer_cast<ltproto::signaling::SignalingMessage>(_msg);
    rtc_server_->onSignalingMessage(msg->rtc_message().key().c_str(),
                                    msg->rtc_message().value().c_str());
}

void WorkerSession::dispatch_signaling_message_core(
    std::shared_ptr<google::protobuf::MessageLite> _msg) {
    auto msg = std::static_pointer_cast<ltproto::signaling::SignalingMessage>(_msg);
    auto& coremsg = msg->core_message();
    LOG(DEBUG) << "Dispatch signaling core message: " << coremsg.key();
    if (coremsg.key() == kSigCoreClose) {
        on_closed(CloseReason::ClientClose);
    }
}

bool WorkerSession::init_pipe_server() {
    ltlib::Server::Params params{};
    params.stype = ltlib::StreamType::Pipe;
    params.ioloop = ioloop_.get();
    params.pipe_name = "\\\\?\\pipe\\" + pipe_name_;
    params.on_accepted = std::bind(&WorkerSession::on_pipe_accepted, this, std::placeholders::_1);
    params.on_closed = std::bind(&WorkerSession::on_pipe_disconnected, this, std::placeholders::_1);
    params.on_message = std::bind(&WorkerSession::on_pipe_message, this, std::placeholders::_1,
                                  std::placeholders::_2, std::placeholders::_3);
    pipe_server_ = ltlib::Server::create(params);
    if (pipe_server_ == nullptr) {
        LOG(WARNING) << "Init pipe server failed";
        return false;
    }
    return true;
}

void WorkerSession::on_pipe_accepted(uint32_t fd) {
    if (pipe_client_fd_ != std::numeric_limits<uint32_t>::max()) {
        LOG(WARNING) << "New worker(" << fd << ") connected to service, but another worker(" << fd
                     << ") already being serve";
        return;
    }
    pipe_client_fd_ = fd;
    LOG(INFO) << "Pipe server accpeted worker(" << fd << ")";
}

void WorkerSession::on_pipe_disconnected(uint32_t fd) {
    if (pipe_client_fd_ != fd) {
        LOG(FATAL) << "Worker(" << fd << ") disconnected, but we are serving worker("
                   << pipe_client_fd_ << ")";
        return;
    }
    pipe_client_fd_ = std::numeric_limits<uint32_t>::max();
    LOGF(INFO, "Worker(%d) disconnected from pipe server", fd);
}

void WorkerSession::on_pipe_message(uint32_t fd, uint32_t type,
                                    std::shared_ptr<google::protobuf::MessageLite> msg) {
    LOGF(DEBUG, "Received pipe message {fd:%u, type:%u}", fd, type);
    if (fd != pipe_client_fd_) {
        LOG(FATAL) << "fd != pipe_client_fd_";
        return;
    }
    namespace ltype = ltproto::type;
    switch (type) {
    case ltype::kStartWorkingAck:
        on_start_working_ack(msg);
        break;
    case ltype::kCaptureVideoFrame:
        on_captured_frame(msg);
        break;
    case ltype::kStreamingParams:
        on_worker_streaming_params(msg);
        break;
    default:
        LOG(WARNING) << "Unknown message type:" << type;
        break;
    }
}

void WorkerSession::start_working() {
    auto msg = std::make_shared<ltproto::peer2peer::StartWorking>();
    send_to_worker(ltproto::id(msg), msg);
}

void WorkerSession::on_start_working_ack(std::shared_ptr<google::protobuf::MessageLite> _msg) {
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
    send_message_to_remote_client(ltproto::id(ack), ack, true);
}

void WorkerSession::send_to_worker(uint32_t type,
                                   std::shared_ptr<google::protobuf::MessageLite> msg) {
    pipe_server_->send(pipe_client_fd_, type, msg);
}

void WorkerSession::on_worker_stoped() {
    ioloop_->post(std::bind(&WorkerSession::on_closed, this, CloseReason::HostClose));
}

void WorkerSession::on_worker_streaming_params(std::shared_ptr<google::protobuf::MessageLite> msg) {
    negotiated_streaming_params_ = msg;
    send_worker_keep_alive();
    maybe_on_create_session_completed();
}

void WorkerSession::send_worker_keep_alive() {
    auto msg = std::make_shared<ltproto::peer2peer::KeepAlive>();
    send_to_worker(ltproto::id(msg), msg);
    ioloop_->post_delay(500, std::bind(&WorkerSession::send_worker_keep_alive, this));
}

void WorkerSession::on_ltrtc_data(const uint8_t* data, uint32_t size, bool reliable) {
    auto type = reinterpret_cast<const uint32_t*>(data);
    // 来自client，发给server，的消息.
    auto msg = ltproto::create_by_type(*type);
    if (msg == nullptr) {
        LOG(WARNING) << "Unknown message type: " << *type;
    }
    bool success = msg->ParseFromArray(data + 4, size - 4);
    if (!success) {
        LOG(WARNING) << "Parse message failed, type: " << *type;
        return;
    }
    dispatch_dc_message(*type, msg);
}

void WorkerSession::on_ltrtc_accepted_thread_safe() {
    if (ioloop_->is_not_current_thread()) {
        ioloop_->post(std::bind(&WorkerSession::on_ltrtc_accepted_thread_safe, this));
        return;
    }
    LOG(INFO) << "Accepted client";
    update_last_recv_time();
    task_thread_->post(std::bind(&WorkerSession::check_timeout, this));
}

void WorkerSession::on_ltrtc_conn_changed() {}

void WorkerSession::on_ltrtc_failed_thread_safe() {
    ioloop_->post(std::bind(&WorkerSession::on_closed, this, CloseReason::TimeoutClose));
}

void WorkerSession::on_ltrtc_disconnected_thread_safe() {
    ioloop_->post(std::bind(&WorkerSession::on_closed, this, CloseReason::TimeoutClose));
}

void WorkerSession::on_ltrtc_signaling_message(const std::string& key, const std::string& value) {
    auto signaling_msg = std::make_shared<ltproto::signaling::SignalingMessage>();
    signaling_msg->set_level(ltproto::signaling::SignalingMessage::Rtc);
    auto rtc_msg = signaling_msg->mutable_rtc_message();
    rtc_msg->set_key(key);
    rtc_msg->set_value(value);
    std::string str = signaling_msg->SerializeAsString();
    if (str.empty()) {
        LOG(WARNING) << "Serialize signaling rtc message failed";
        return;
    }
    ioloop_->post([this, signaling_msg]() {
        signaling_client_->send(ltproto::id(signaling_msg), signaling_msg);
    });
}

void WorkerSession::on_captured_frame(std::shared_ptr<google::protobuf::MessageLite> _msg) {
    // NOTE: 这是在IOLoop线程
    auto captured_frame = std::static_pointer_cast<ltproto::peer2peer::CaptureVideoFrame>(_msg);
    auto encoded_frame = video_encoder_->encode(captured_frame, false);
    if (encoded_frame.is_black_frame) {
        //???
    }
    rtc_server_->sendVideo(encoded_frame);
    // static std::ofstream out{"./service_stream", std::ios::binary};
    // out.write(reinterpret_cast<const char*>(encoded_frame.data), encoded_frame.size);
    // out.flush();
}

void WorkerSession::dispatch_dc_message(uint32_t type,
                                        const std::shared_ptr<google::protobuf::MessageLite>& msg) {
    update_last_recv_time();
    namespace ltype = ltproto::type;
    switch (type) {
    case ltype::kKeepAlive:
        on_keep_alive(msg);
        break;
    case ltype::kStartTransmission:
        on_start_transmission(msg);
        break;
    default:
        if (worker_registered_msg_.find(type) != worker_registered_msg_.cend()) {
            send_to_worker(type, msg);
        }
        break;
    }
}

void WorkerSession::on_start_transmission(std::shared_ptr<google::protobuf::MessageLite> _msg) {
    auto ack = std::make_shared<ltproto::peer2peer::StartTransmissionAck>();
    if (client_connected_) {
        ack->set_err_code(ltproto::peer2peer::StartTransmissionAck_ErrCode_Success);
        send_message_to_remote_client(ltproto::id(ack), ack, true);
        return;
    }
    client_connected_ = true;
    auto msg = std::static_pointer_cast<ltproto::peer2peer::StartTransmission>(_msg);
    if (msg->token() != auth_token_) {
        LOG(WARNING) << "Received SetupConnection with invalid token: " << msg->token();
        ack->set_err_code(ltproto::peer2peer::StartTransmissionAck_ErrCode_AuthFailed);
        send_message_to_remote_client(ltproto::id(ack), ack, true);
        return;
    }
    start_working();
    // 暂时不回Ack，等到worker process回了StartWorkingAck再回.
}

void WorkerSession::on_keep_alive(std::shared_ptr<google::protobuf::MessageLite> msg) {
    // 是否需要回ack
}

void WorkerSession::update_last_recv_time() {
    last_recv_time_us_ = ltlib::steady_now_us();
}

void WorkerSession::check_timeout() {
    constexpr auto kTimeoutUS = 3000'000;
    auto now = ltlib::steady_now_us();
    if (now - last_recv_time_us_ > kTimeoutUS) {
        rtc_server_->close();
        // FIXME: on_closed必须在ioloop线程调用，这里用错了
        on_closed(CloseReason::TimeoutClose);
    }
    else {
        task_thread_->post_delay(ltlib::TimeDelta{kTimeoutUS},
                                 std::bind(&WorkerSession::check_timeout, this));
    }
}

bool WorkerSession::send_message_to_remote_client(
    uint32_t type, const std::shared_ptr<google::protobuf::MessageLite>& msg, bool reliable) {
    auto packet = ltproto::Packet::create({type, msg}, false);
    if (!packet.has_value()) {
        LOG(WARNING) << "Create Peer2Peer packet failed, type:" << type;
        return false;
    }
    const auto& pkt = packet.value();
    // rtc的数据通道可以帮助我们完成stream->packet的过程，所以这里不需要把packet header一起传过去.
    bool success = rtc_server_->sendData(pkt.payload.get(), pkt.header.payload_size, reliable);
    return success;
}

} // namespace svc

} // namespace lt