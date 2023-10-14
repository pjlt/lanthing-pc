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

#include "service.h"
#include <cassert>

#include <ltlib/logging.h>

#include <ltlib/strings.h>
#include <ltproto/ltproto.h>
#include <ltproto/server/close_connection.pb.h>
#include <ltproto/server/login_device.pb.h>
#include <ltproto/server/login_device_ack.pb.h>
#include <ltproto/server/open_connection.pb.h>
#include <ltproto/server/open_connection_ack.pb.h>
#include <ltproto/service2app/confirm_connection.pb.h>
#include <ltproto/service2app/confirm_connection_ack.pb.h>
#include <ltproto/service2app/disconnected_connection.pb.h>

namespace lt {

namespace svc {

Service::Service() = default;

Service::~Service() {
    {
        std::lock_guard lock{mutex_};
        tcp_client_.reset();
        ioloop_.reset();
    }
}

bool Service::init() {
    if (!initSettings()) {
        return false;
    }
    std::optional<int64_t> device_id = settings_->getInteger("device_id");
    if (!device_id.has_value() || device_id.value() == 0) {
        LOG(ERR) << "Get device_id from local settings failed";
        return false;
    }
    device_id_ = device_id.value();

    ioloop_ = ltlib::IOLoop::create();
    if (ioloop_ == nullptr) {
        return false;
    }
    if (!initTcpClient()) {
        return false;
    }
    if (!initAppClient()) {
        return false;
    }
    std::promise<void> promise;
    auto future = promise.get_future();
    thread_ = ltlib::BlockingThread::create(
        "main_thread", [this, &promise](const std::function<void()>& i_am_alive) {
            promise.set_value();
            mainLoop(i_am_alive);
        });
    future.get();
    return true;
}

void Service::uninit() {
    //
}

#define MACRO_TO_STRING_HELPER(str) #str
#define MACRO_TO_STRING(str) MACRO_TO_STRING_HELPER(str)
#include <lanthing.cert>
bool Service::initTcpClient() {
    ltlib::Client::Params params{};
    params.stype = ltlib::StreamType::TCP;
    params.ioloop = ioloop_.get();
    params.host = MACRO_TO_STRING(LT_SERVER_ADDR);
    params.port = LT_SERVER_SVC_PORT;
    params.is_tls = LT_SERVER_USE_SSL;
    params.cert = kLanthingCert;
    params.on_connected = std::bind(&Service::onServerConnected, this);
    params.on_closed = std::bind(&Service::onServerDisconnected, this);
    params.on_reconnecting = std::bind(&Service::onServerReconnecting, this);
    params.on_message =
        std::bind(&Service::onServerMessage, this, std::placeholders::_1, std::placeholders::_2);
    tcp_client_ = ltlib::Client::create(params);
    if (tcp_client_ == nullptr) {
        return false;
    }
    return true;
}
#undef MACRO_TO_STRING
#undef MACRO_TO_STRING_HELPER

bool Service::initAppClient() {
    ltlib::Client::Params params{};
    params.stype = ltlib::StreamType::Pipe;
    params.ioloop = ioloop_.get();
    params.pipe_name = "\\\\?\\pipe\\lanthing_service_manager";
    params.is_tls = false;
    params.on_closed = std::bind(&Service::onAppDisconnected, this);
    params.on_connected = std::bind(&Service::onAppConnected, this);
    params.on_message =
        std::bind(&Service::onAppMessage, this, std::placeholders::_1, std::placeholders::_2);
    params.on_reconnecting = std::bind(&Service::onAppReconnecting, this);
    app_client_ = ltlib::Client::create(params);
    return app_client_ != nullptr;
}

void Service::mainLoop(const std::function<void()>& i_am_alive) {
    LOG(INFO) << "Lanthing service enter main loop";
    ioloop_->run(i_am_alive);
    LOG(INFO) << "Lanthing service exit main loop";
}

bool Service::initSettings() {
    settings_ = ltlib::Settings::create(ltlib::Settings::Storage::Sqlite);
    return settings_ != nullptr;
}

// TODO: 删除锁，确保worker_sessions_只在ioloop中使用
void Service::createSession(const WorkerSession::Params& params) {
    auto session = WorkerSession::create(params);
    if (session != nullptr) {
        std::lock_guard<std::mutex> lock{mutex_};
        worker_sessions_[params.name] = session;
    }
    else {
        auto ack = std::make_shared<ltproto::server::OpenConnectionAck>();
        ack->set_err_code(ltproto::server::OpenConnectionAck_ErrCode_Invalid);
        tcp_client_->send(ltproto::id(ack), ack);
        // 删除占位的nullptr
        std::lock_guard<std::mutex> lock{mutex_};
        worker_sessions_.erase(params.name);
    }
}

void Service::destroySession(const std::string& session_name) {
    // worker_sessions_.erase(session_name)会析构WorkerSession内部的PeerConnection
    // 而当前的destroy_session()很可能是PeerConnection信令线程回调上来的
    // 这里选择放到libuv的线程去做
    postTask([this, session_name]() { worker_sessions_.erase(session_name); });
}

void Service::letUserConfirm(int64_t device_id) {
    if (!app_connected_) {
        LOG(WARNING) << "App not online, can't confirm connection";
        auto ack = std::make_shared<ltproto::server::OpenConnectionAck>();
        ack->set_err_code(ltproto::server::OpenConnectionAck_ErrCode_Invalid);
        tcp_client_->send(ltproto::id(ack), ack);
        worker_sessions_.erase(cached_worker_params_->name);
        cached_worker_params_ = std::nullopt;
        return;
    }
    auto msg = std::make_shared<ltproto::service2app::ConfirmConnection>();
    msg->set_device_id(device_id);
    sendMessageToApp(ltproto::id(msg), msg);
}

void Service::postTask(const std::function<void()>& task) {
    std::lock_guard lock{mutex_};
    if (ioloop_) {
        ioloop_->post(task);
    }
}

void Service::postDelayTask(int64_t delay_ms, const std::function<void()>& task) {
    std::lock_guard lock{mutex_};
    if (ioloop_) {
        ioloop_->postDelay(delay_ms, task);
    }
}

void Service::onServerMessage(uint32_t type, std::shared_ptr<google::protobuf::MessageLite> msg) {
    namespace ltype = ltproto::type;
    switch (type) {
    case ltype::kLoginDeviceAck:
        onLoginDeviceAck(msg);
        break;
    case ltype::kLoginUser:
        onLoginUserAck(msg);
        break;
    case ltype::kOpenConnection:
        onOpenConnection(msg);
        break;
    default:
        LOG(WARNING) << "Unknown message from server " << type;
        break;
    }
}

void Service::onServerDisconnected() {
    // 怎么办？
}

void Service::onServerReconnecting() {
    LOG(INFO) << "Reconnecting to lanthing server...";
}

void Service::onServerConnected() {
    LOG(INFO) << "Connected to server";
    loginDevice();
}

void Service::onOpenConnection(std::shared_ptr<google::protobuf::MessageLite> _msg) {
    LOG(INFO) << "Received OpenConnection";
    // 1. 校验参数
    auto msg = std::static_pointer_cast<ltproto::server::OpenConnection>(_msg);
    auto ack = std::make_shared<ltproto::server::OpenConnectionAck>();
    if (msg->client_device_id() <= 0) {
        ack->set_err_code(ltproto::server::OpenConnectionAck_ErrCode_Invalid);
        tcp_client_->send(ltproto::id(ack), ack);
        LOG(ERR) << "Invalid device id " << msg->client_device_id();
    }
    std::optional<std::string> access_token = settings_->getString("access_token");
    if (!access_token.has_value() || access_token.value().empty()) {
        ack->set_err_code(ltproto::server::OpenConnectionAck_ErrCode_Invalid);
        tcp_client_->send(ltproto::id(ack), ack);
        LOG(ERR) << "Get access_token from local settings failed";
        return;
    }
    if (msg->access_token() != access_token.value()) {
        ack->set_err_code(ltproto::server::OpenConnectionAck_ErrCode_Invalid);
        tcp_client_->send(ltproto::id(ack), ack);
        LOG(ERR) << "Received connection with invalid access_token: " << msg->access_token();
        return;
    }
    constexpr size_t kSessionNameLen = 8;
    const std::string session_name = ltlib::randomStr(kSessionNameLen);
    {
        std::lock_guard<std::mutex> lock{mutex_};
        if (!worker_sessions_.empty()) {
            LOG(ERR) << "Only support one client";
            return;
        }
        else {
            // 用一个nullptr占位，即使释放锁，其他线程也不会modify这个worker_sessions_
            worker_sessions_[session_name] = nullptr;
        }
    }
    // 2. 准备启动worker的参数
    WorkerSession::Params worker_params{};
    worker_params.name = session_name;
    worker_params.ioloop = ioloop_.get();
    worker_params.post_task = std::bind(&Service::postTask, this, std::placeholders::_1);
    worker_params.post_delay_task =
        std::bind(&Service::postDelayTask, this, std::placeholders::_1, std::placeholders::_2);
    worker_params.user_defined_relay_server = settings_->getString("relay").value_or("");
    worker_params.msg = msg;
    worker_params.on_create_completed =
        std::bind(&Service::onCreateSessionCompletedThreadSafe, this, std::placeholders::_1,
                  std::placeholders::_2, std::placeholders::_3);
    worker_params.on_closed =
        std::bind(&Service::onSessionClosedThreadSafe, this, std::placeholders::_1,
                  std::placeholders::_2, std::placeholders::_3, std::placeholders::_4);
    worker_params.on_accepted_client =
        std::bind(&Service::onAcceptedConnection, this, std::placeholders::_1);
    worker_params.on_client_status =
        std::bind(&Service::onConnectionStatus, this, std::placeholders::_1);
    cached_worker_params_ = worker_params;
    // 3. 校验cookie，通过则直接启动worker，不通过则弹窗让用户确认
    std::string cookie_name = "from_" + std::to_string(msg->client_device_id());
    constexpr int64_t kSecondsPerWeek = int64_t(60) * 60 * 24 * 7;
    const int64_t now = ltlib::utc_now_ms() / 1000; // sqlite的时间戳是UTC+0
    auto update_at = settings_->getUpdateTime(cookie_name);
    if (!update_at.has_value() || now > update_at.value() + kSecondsPerWeek) {
        letUserConfirm(msg->client_device_id());
        return;
    }
    auto cookie = settings_->getString(cookie_name);
    if (!cookie.has_value() || cookie.value() != msg->cookie()) {
        letUserConfirm(msg->client_device_id());
        return;
    }
    // 更新时间戳
    settings_->setString(cookie_name, cookie.value());
    createSession(worker_params);
}

void Service::onLoginDeviceAck(std::shared_ptr<google::protobuf::MessageLite> msg) {
    auto ack = std::static_pointer_cast<ltproto::server::LoginDeviceAck>(msg);
    LOG(INFO) << "LoginDeviceAck: "
              << ltproto::server::LoginDeviceAck::ErrCode_Name(ack->err_code());
}

void Service::onLoginUserAck(std::shared_ptr<google::protobuf::MessageLite> msg) {
    (void)msg;
}

void Service::onCreateSessionCompletedThreadSafe(
    bool success, const std::string& session_name,
    std::shared_ptr<google::protobuf::MessageLite> params) {
    postTask(std::bind(&Service::onCreateSessionCompleted, this, success, session_name, params));
}

void Service::onCreateSessionCompleted(bool success, const std::string& session_name,
                                       std::shared_ptr<google::protobuf::MessageLite> params) {
    (void)session_name;
    auto ack = std::make_shared<ltproto::server::OpenConnectionAck>();
    if (success) {
        ack->set_err_code(ltproto::server::OpenConnectionAck_ErrCode_Success);
        auto streaming_params = ack->mutable_streaming_params();
        auto negotiated_params = std::static_pointer_cast<ltproto::common::StreamingParams>(params);
        streaming_params->CopyFrom(*negotiated_params);
    }
    else {
        ack->set_err_code(ltproto::server::OpenConnectionAck_ErrCode_Invalid);
    }
    tcp_client_->send(ltproto::id(ack), ack);
}

void Service::onSessionClosedThreadSafe(int64_t device_id, WorkerSession::CloseReason close_reason,
                                        const std::string& session_name,
                                        const std::string& room_id) {
    postTask(
        std::bind(&Service::onSessionClosed, this, device_id, close_reason, session_name, room_id));
}

void Service::onSessionClosed(int64_t device_id, WorkerSession::CloseReason close_reason,
                              const std::string& session_name, const std::string& room_id) {
    reportSessionClosed(close_reason, room_id);
    destroySession(session_name);
    tellAppSessionClosed(device_id);
}

void Service::sendMessageToServer(uint32_t type,
                                  std::shared_ptr<google::protobuf::MessageLite> msg) {
    tcp_client_->send(type, msg);
}

void Service::loginDevice() {
    std::optional<bool> allow_control = settings_->getBoolean("allow_control");
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

void Service::loginUser() {}

void Service::reportSessionClosed(WorkerSession::CloseReason close_reason,
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

void Service::onAppMessage(uint32_t type, std::shared_ptr<google::protobuf::MessageLite> msg) {
    namespace ltype = ltproto::type;
    switch (type) {
    case ltype::kConfirmConnectionAck:
        onConfirmConnectionAck(msg);
        break;
    default:
        LOG(WARNING) << "Unknown message from app " << type;
        break;
    }
}

void Service::onAppDisconnected() {
    LOG(INFO) << "Disconnected from App";
    app_connected_ = false;
}

void Service::onAppReconnecting() {
    if (app_connected_) {
        LOG(INFO) << "Reconnecting to App...";
        app_connected_ = false;
    }
}

void Service::onAppConnected() {
    LOG(INFO) << "Connected to App";
    app_connected_ = true;
}

void Service::sendMessageToApp(uint32_t type, std::shared_ptr<google::protobuf::MessageLite> msg) {
    if (app_connected_) {
        app_client_->send(type, msg);
    }
}

void Service::onConfirmConnectionAck(std::shared_ptr<google::protobuf::MessageLite> _msg) {
    if (!cached_worker_params_.has_value()) {
        LOG(ERR) << "Cached WorkerParams is empty";
        auto ack = std::make_shared<ltproto::server::OpenConnectionAck>();
        ack->set_err_code(ltproto::server::OpenConnectionAck_ErrCode_Invalid);
        tcp_client_->send(ltproto::id(ack), ack);
        return;
    }
    auto params = cached_worker_params_.value();
    cached_worker_params_ = std::nullopt;
    auto msg = std::static_pointer_cast<ltproto::service2app::ConfirmConnectionAck>(_msg);
    switch (msg->result()) {
    case ltproto::service2app::ConfirmConnectionAck_ConfirmResult_Agree:
        createSession(params);
        break;
    case ltproto::service2app::ConfirmConnectionAck_ConfirmResult_Reject:
    {
        worker_sessions_.erase(params.name);
        auto ack = std::make_shared<ltproto::server::OpenConnectionAck>();
        ack->set_err_code(ltproto::server::OpenConnectionAck_ErrCode_Invalid); // TODO: reject code
        tcp_client_->send(ltproto::id(ack), ack);
        break;
    }
    case ltproto::service2app::ConfirmConnectionAck_ConfirmResult_AgreeNextTime:
    {
        auto req = std::static_pointer_cast<ltproto::server::OpenConnection>(params.msg);
        std::string cookie_name = "from_" + std::to_string(req->client_device_id());
        settings_->setString(cookie_name, req->cookie());
        createSession(params);
        break;
    }
    default:
        LOG(ERR) << "Unknown ConfirmResult " << (int)msg->result() << ", treat as rejct";
        worker_sessions_.erase(params.name);
        auto ack = std::make_shared<ltproto::server::OpenConnectionAck>();
        ack->set_err_code(ltproto::server::OpenConnectionAck_ErrCode_Invalid); // TODO: reject code
        tcp_client_->send(ltproto::id(ack), ack);
        break;
    }
}

void Service::tellAppSessionClosed(int64_t device_id) {
    auto msg = std::make_shared<ltproto::service2app::DisconnectedConnection>();
    msg->set_device_id(device_id);
    sendMessageToApp(ltproto::id(msg), msg);
}

void Service::onAcceptedConnection(std::shared_ptr<google::protobuf::MessageLite> msg) {
    sendMessageToApp(ltproto::type::kAcceptedConnection, msg);
}

void Service::onConnectionStatus(std::shared_ptr<google::protobuf::MessageLite> msg) {
    sendMessageToApp(ltproto::type::kConnectionStatus, msg);
}

} // namespace svc

} // namespace lt