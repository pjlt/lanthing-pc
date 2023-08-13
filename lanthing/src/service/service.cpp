#include "service.h"
#include <cassert>

#include <g3log/g3log.hpp>

#include <ltlib/strings.h>
#include <ltproto/ltproto.h>
#include <ltproto/server/close_connection.pb.h>
#include <ltproto/server/login_device.pb.h>
#include <ltproto/server/login_device_ack.pb.h>
#include <ltproto/server/open_connection.pb.h>
#include <ltproto/server/open_connection_ack.pb.h>

namespace lt {

namespace svc {

Service::Service() = default;

Service::~Service() {
    assert(ioloop_->is_not_current_thread());
    if (ioloop_) {
        ioloop_->stop();
    }
}

bool Service::init() {
    if (!init_settings()) {
        return false;
    }
    std::optional<int64_t> device_id = settings_->get_integer("device_id");
    if (!device_id.has_value() || device_id.value() == 0) {
        LOG(WARNING) << "Get device_id from local settings failed";
        return false;
    }
    device_id_ = device_id.value();

    // std::optional<std::string> access_token = settings_->get_string("access_token");
    // if (!access_token.has_value() || access_token.value().empty()) {
    //     LOG(WARNING) << "Get access_token from local settings failed";
    //     return false;
    // }
    // access_token_ = access_token.value();

    ioloop_ = ltlib::IOLoop::create();
    if (ioloop_ == nullptr) {
        return false;
    }
    if (!init_tcp_client()) {
        return false;
    }
    std::promise<void> promise;
    auto future = promise.get_future();
    thread_ = ltlib::BlockingThread::create(
        "main_thread",
        [this, &promise](const std::function<void()>& i_am_alive, void*) {
            promise.set_value();
            main_loop(i_am_alive);
        },
        nullptr);
    future.get();
    return true;
}

void Service::uninit() {
    //
}

#define MACRO_TO_STRING_HELPER(str) #str
#define MACRO_TO_STRING(str) MACRO_TO_STRING_HELPER(str)
#include <lanthing.cert>
bool Service::init_tcp_client() {
    ltlib::Client::Params params{};
    params.stype = ltlib::StreamType::TCP;
    params.ioloop = ioloop_.get();
    params.host = MACRO_TO_STRING(LT_SERVER_ADDR);
    params.port = LT_SERVER_SVC_PORT;
    params.is_tls = LT_SERVER_USE_SSL;
    params.cert = kLanthingCert;
    params.on_connected = std::bind(&Service::on_server_connected, this);
    params.on_closed = std::bind(&Service::on_server_disconnected, this);
    params.on_reconnecting = std::bind(&Service::on_server_reconnecting, this);
    params.on_message =
        std::bind(&Service::on_server_message, this, std::placeholders::_1, std::placeholders::_2);
    tcp_client_ = ltlib::Client::create(params);
    if (tcp_client_ == nullptr) {
        return false;
    }
    return true;
}
#undef MACRO_TO_STRING
#undef MACRO_TO_STRING_HELPER

void Service::main_loop(const std::function<void()>& i_am_alive) {
    LOG(INFO) << "Lanthing service enter main loop";
    ioloop_->run(i_am_alive);
}

bool Service::init_settings() {
    settings_ = ltlib::Settings::create(ltlib::Settings::Storage::Toml);
    return settings_ != nullptr;
}

void Service::destroy_session(const std::string& session_name) {
    // worker_sessions_.erase(session_name)会析构WorkerSession内部的PeerConnection
    // 而当前的destroy_session()很可能是PeerConnection信令线程回调上来的
    // 这里选择放到libuv的线程去做
    if (ioloop_->is_not_current_thread()) {
        ioloop_->post(std::bind(&Service::destroy_session, this, session_name));
    }
    else {
        worker_sessions_.erase(session_name);
    }
}

void Service::on_server_message(uint32_t type, std::shared_ptr<google::protobuf::MessageLite> msg) {
    dispatch_server_message(type, msg);
}

void Service::dispatch_server_message(uint32_t type,
                                      std::shared_ptr<google::protobuf::MessageLite> msg) {
    namespace ltype = ltproto::type;
    switch (type) {
    case ltype::kLoginDeviceAck:
        on_login_device_ack(msg);
        break;
    case ltype::kLoginUser:
        on_login_user_ack(msg);
        break;
    case ltype::kOpenConnection:
        on_open_connection(msg);
        break;
    default:
        LOG(WARNING) << "Unknown message from server " << type;
        break;
    }
}

void Service::on_server_disconnected() {
    // 怎么办？
}

void Service::on_server_reconnecting() {
    LOG(INFO) << "Reconnecting to lanthing server...";
}

void Service::on_server_connected() {
    LOG(INFO) << "Connected to server";
    login_device();
}

void Service::on_open_connection(std::shared_ptr<google::protobuf::MessageLite> _msg) {
    LOG(INFO) << "Received OpenConnection";
    auto msg = std::static_pointer_cast<ltproto::server::OpenConnection>(_msg);
    auto ack = std::make_shared<ltproto::server::OpenConnectionAck>();
    std::optional<std::string> access_token = settings_->get_string("access_token");
    if (!access_token.has_value() || access_token.value().empty()) {
        LOG(WARNING) << "Get access_token from local settings failed";
        return;
    }
    if (msg->access_token() != access_token.value()) {
        ack->set_err_code(ltproto::server::OpenConnectionAck_ErrCode_Invalid);
        tcp_client_->send(ltproto::id(ack), ack);
        LOG(WARNING) << "Received connection with invalid access_token: " << msg->access_token();
        return;
    }
    constexpr size_t kSessionNameLen = 8;
    const std::string session_name = ltlib::random_str(kSessionNameLen);
    {
        std::lock_guard<std::mutex> lock{mutex_};
        if (!worker_sessions_.empty()) {
            LOG(WARNING) << "Only support one client";
            return;
        }
        else {
            // 用一个nullptr占位，即使释放锁，其他线程也不会modify这个worker_sessions_
            worker_sessions_[session_name] = nullptr;
        }
    }
    WorkerSession::Params worker_params{};
    worker_params.name = session_name;
    worker_params.user_defined_relay_server = settings_->get_string("relay").value_or("");
    worker_params.msg = msg;
    worker_params.on_create_completed =
        std::bind(&Service::on_create_session_completed_thread_safe, this, std::placeholders::_1,
                  std::placeholders::_2, std::placeholders::_3);
    worker_params.on_closed =
        std::bind(&Service::on_session_closed_thread_safe, this, std::placeholders::_1,
                  std::placeholders::_2, std::placeholders::_3);
    auto session = WorkerSession::create(worker_params);
    if (session != nullptr) {
        std::lock_guard<std::mutex> lock{mutex_};
        worker_sessions_[session_name] = session;
    }
    else {
        ack->set_err_code(ltproto::server::OpenConnectionAck_ErrCode_Invalid);
        tcp_client_->send(ltproto::id(ack), ack);
        // 删除占位的nullptr
        std::lock_guard<std::mutex> lock{mutex_};
        worker_sessions_.erase(session_name);
    }
}

void Service::on_login_device_ack(std::shared_ptr<google::protobuf::MessageLite> msg) {
    auto ack = std::static_pointer_cast<ltproto::server::LoginDeviceAck>(msg);
    LOG(INFO) << "LoginDeviceAck: "
              << ltproto::server::LoginDeviceAck::ErrCode_Name(ack->err_code());
}

void Service::on_login_user_ack(std::shared_ptr<google::protobuf::MessageLite> msg) {}

void Service::on_create_session_completed_thread_safe(
    bool success, const std::string& session_name,
    std::shared_ptr<google::protobuf::MessageLite> params) {
    if (ioloop_->is_not_current_thread()) {
        ioloop_->post(std::bind(&Service::on_create_session_completed_thread_safe, this, success,
                                session_name, params));
        return;
    }
    auto ack = std::make_shared<ltproto::server::OpenConnectionAck>();
    if (success) {
        ack->set_err_code(ltproto::server::OpenConnectionAck_ErrCode_Success);
        auto streaming_params = ack->mutable_streaming_params();
        auto negotiated_params =
            std::static_pointer_cast<ltproto::peer2peer::StreamingParams>(params);
        streaming_params->CopyFrom(*negotiated_params);
    }
    else {
        ack->set_err_code(ltproto::server::OpenConnectionAck_ErrCode_Invalid);
    }
    tcp_client_->send(ltproto::id(ack), ack);
}

void Service::on_session_closed_thread_safe(WorkerSession::CloseReason close_reason,
                                            const std::string& session_name,
                                            const std::string& room_id) {
    if (ioloop_->is_not_current_thread()) {
        ioloop_->post(std::bind(&Service::on_session_closed_thread_safe, this, close_reason,
                                session_name, room_id));
        return;
    }
    report_session_closed(close_reason, room_id);
    destroy_session(session_name);
}

void Service::send_message_to_server(uint32_t type,
                                     std::shared_ptr<google::protobuf::MessageLite> msg) {
    tcp_client_->send(type, msg);
}

void Service::login_device() {
    std::optional<bool> allow_control = settings_->get_boolean("allow_control");
    auto msg = std::make_shared<ltproto::server::LoginDevice>();
    msg->set_device_id(device_id_);
    if (allow_control.has_value()) {
        msg->set_allow_control(allow_control.value());
    }
    else {
        msg->set_allow_control(false);
    }
    tcp_client_->send(ltproto::id(msg), msg);
}

void Service::login_user() {}

void Service::report_session_closed(WorkerSession::CloseReason close_reason,
                                    const std::string& room_id) {
    auto msg = std::make_shared<ltproto::server::CloseConnection>();
    auto reason = ltproto::server::CloseConnection_Reason_TimeoutClose;
    switch (close_reason) {
    case WorkerSession::CloseReason::ClientClose:
        reason = ltproto::server::CloseConnection_Reason_ClientClose;
        break;
    case WorkerSession::CloseReason::HostClose:
        reason = ltproto::server::CloseConnection_Reason_HostClose;
        break;
    case WorkerSession::CloseReason::TimeoutClose:
        reason = ltproto::server::CloseConnection_Reason_ClientClose;
        break;
    default:
        break;
    }
    msg->set_reason(reason);
    msg->set_room_id(room_id);
    tcp_client_->send(ltproto::id(msg), msg);
}

} // namespace svc

} // namespace lt