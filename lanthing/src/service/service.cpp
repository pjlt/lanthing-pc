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
#include <ltlib/versions.h>
#include <ltlib/win_service.h>

#include <ltproto/common/keep_alive.pb.h>
#include <ltproto/ltproto.h>
#include <ltproto/server/close_connection.pb.h>
#include <ltproto/server/login_device.pb.h>
#include <ltproto/server/login_device_ack.pb.h>
#include <ltproto/server/open_connection.pb.h>
#include <ltproto/server/open_connection_ack.pb.h>
#include <ltproto/service2app/confirm_connection.pb.h>
#include <ltproto/service2app/confirm_connection_ack.pb.h>
#include <ltproto/service2app/disconnected_connection.pb.h>
#include <ltproto/service2app/operate_connection.pb.h>
#include <ltproto/service2app/service_status.pb.h>

#include <lt_constants.h>

namespace lt {

namespace svc {

Service::Service() = default;

Service::~Service() {
    {
        std::lock_guard lock{mutex_};
        stoped_ = true;
        tcp_client_.reset();
        app_client_.reset();
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
        "lt_main_thread", [this, &promise](const std::function<void()>& i_am_alive) {
            promise.set_value();
            mainLoop(i_am_alive);
        });
    future.get();
    postDelayTask(1000, std::bind(&Service::checkRunAsService, this));
    return true;
}

void Service::uninit() {
    //
}

#define MACRO_TO_STRING_HELPER(str) #str
#define MACRO_TO_STRING(str) MACRO_TO_STRING_HELPER(str)
#include <trusted-root.cert>
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

void Service::createSession(const WorkerSession::Params& params) {
    auto session = WorkerSession::create(params);
    if (session != nullptr) {
        worker_sessions_[params.name] = session;
    }
    else {
        auto ack = std::make_shared<ltproto::server::OpenConnectionAck>();
        ack->set_err_code(ltproto::ErrorCode::Unknown);
        tcp_client_->send(ltproto::id(ack), ack);
        // 删除占位的nullptr
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
        ack->set_err_code(ltproto::ErrorCode::AppNotOnline);
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
    // 不断尝试获取锁，获取失败有两种可能
    // 1. ~Service获取了exclusive锁
    // 2. 意外的失败
    // 对于第一种可能，~Service获取锁后会赋值stoped_=true，在此处检测到stoped_==true，我们就退出
    // 对于第二种可能，我们循环尝试获取锁
    // 另外，~Service获取锁和赋值stoped_=true并不是原子，也就是说第一种情况下stoped_有可能是false
    // 但是没关系，我们继续循环
    while (!mutex_.try_lock_shared()) {
        if (stoped_) {
            return;
        }
    }
    if (ioloop_) {
        ioloop_->post(task);
    }
    mutex_.unlock_shared();
}

void Service::postDelayTask(int64_t delay_ms, const std::function<void()>& task) {
    // 同postTask
    while (!mutex_.try_lock_shared()) {
        if (stoped_) {
            return;
        }
    }
    if (ioloop_) {
        ioloop_->postDelay(delay_ms, task);
    }
    mutex_.unlock_shared();
}

void Service::checkRunAsService() {
#if LT_RUN_AS_SERVICE
    if (app_connected_) {
        app_not_connected_count_ = 0;
    }
    else {
        app_not_connected_count_ += 1;
        if (app_not_connected_count_ >= 2) {
            // 暂时屏蔽该功能，不支持无人值守
            // std::optional<bool> run_as_daemon = settings_->getBoolean("daemon");
            std::optional<bool> run_as_daemon = false;
            // 值未填、或明确设置为否，则退出进程
            if (!run_as_daemon.has_value() || *run_as_daemon == false) {
                LOG(INFO) << "checkRunAsService exit";
                const std::string service_name = "Lanthing";
                ltlib::ServiceCtrl::stopService(service_name);
            }
        }
    }
    postDelayTask(1000, std::bind(&Service::checkRunAsService, this));
#endif
}

void Service::onServerMessage(uint32_t type, std::shared_ptr<google::protobuf::MessageLite> msg) {
    namespace ltype = ltproto::type;
    switch (type) {
    case ltype::kKeepAliveAck:
        // do nothing
        break;
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
    server_logged_ = false;
    LOG(ERR) << "Disconnected from server";

    auto status = std::make_shared<ltproto::service2app::ServiceStatus>();
    status->set_status(ltproto::ErrorCode::ServiceStatusDisconnectedFromServer);
    sendMessageToApp(ltproto::id(status), status);
}

void Service::onServerReconnecting() {
    server_logged_ = false;
    LOG(INFO) << "Reconnecting to lanthing server...";

    auto status = std::make_shared<ltproto::service2app::ServiceStatus>();
    status->set_status(ltproto::ErrorCode::ServiceStatusDisconnectedFromServer);
    sendMessageToApp(ltproto::id(status), status);
}

void Service::onServerConnected() {
    LOG(INFO) << "Connected to server";
    loginDevice();
    if (!keepalive_inited_) {
        keepalive_inited_ = true;
        sendKeepAliveToServer();
    }
}

void Service::onOpenConnection(std::shared_ptr<google::protobuf::MessageLite> _msg) {
    LOG(INFO) << "Received OpenConnection";
    // 1. 校验参数
    auto msg = std::static_pointer_cast<ltproto::server::OpenConnection>(_msg);
    auto ack = std::make_shared<ltproto::server::OpenConnectionAck>();
    int64_t client_version = msg->client_version();
    int64_t client_required_version = msg->required_version();
    int64_t my_version =
        ltlib::combineVersion(LT_VERSION_MAJOR, LT_VERSION_MINOR, LT_VERSION_PATCH);
    int64_t my_required_version = ltlib::combineVersion(0, 3, 3);
    if (client_version < my_required_version) {
        ack->set_err_code(ltproto::ErrorCode::ClientVresionTooLow);
        tcp_client_->send(ltproto::id(ack), ack);
        LOG(ERR) << "Client version too low";
    }
    else if (my_version < client_required_version) {
        ack->set_err_code(ltproto::ErrorCode::HostVersionTooLow);
        tcp_client_->send(ltproto::id(ack), ack);
        LOG(ERR) << "Host version too low";
    }
    if (msg->client_device_id() <= 0) {
        ack->set_err_code(ltproto::ErrorCode::InvalidParameter);
        tcp_client_->send(ltproto::id(ack), ack);
        LOG(ERR) << "Invalid device id " << msg->client_device_id();
    }
    std::optional<std::string> access_token = settings_->getString("access_token");
    if (!access_token.has_value() || access_token.value().empty()) {
        ack->set_err_code(ltproto::ErrorCode::AccessCodeInvalid);
        tcp_client_->send(ltproto::id(ack), ack);
        LOG(ERR) << "Get access_token from local settings failed";
        return;
    }
    if (msg->access_token() != access_token.value()) {
        ack->set_err_code(ltproto::ErrorCode::AccessCodeInvalid);
        tcp_client_->send(ltproto::id(ack), ack);
        LOG(ERR) << "Received connection with invalid access_token: " << msg->access_token();
        return;
    }
    constexpr size_t kSessionNameLen = 8;
    const std::string session_name = ltlib::randomStr(kSessionNameLen);
    if (!worker_sessions_.empty()) {
        ack->set_err_code(ltproto::ErrorCode::ServingAnotherClient);
        tcp_client_->send(ltproto::id(ack), ack);
        LOG(ERR) << "Only support one client";
        return;
    }
    else {
        // 用一个nullptr占位
        worker_sessions_[session_name] = nullptr;
    }
    // 2. 准备启动worker的参数
    std::string id_str = std::to_string(msg->client_device_id());
    uint16_t min_port = static_cast<uint16_t>(settings_->getInteger("min_port").value_or(0));
    uint16_t max_port = static_cast<uint16_t>(settings_->getInteger("max_port").value_or(0));

    WorkerSession::Params worker_params{};
    worker_params.name = session_name;
    worker_params.enable_gamepad =
        settings_->getBoolean("enable_gamepad_for_" + id_str).value_or(true);
    worker_params.enable_keyboard =
        settings_->getBoolean("enable_keyboard_for_" + id_str).value_or(false);
    worker_params.enable_mouse =
        settings_->getBoolean("enable_mouse_for_" + id_str).value_or(false);
    // NOTE: 这种写法会把RTC2擦除
    bool host_enable_tcp = settings_->getBoolean("enable_tcp").value_or(false);
    bool client_enable_tcp = msg->transport_type() == ltproto::common::TCP;
    if (msg->transport_type() == ltproto::common::TransportType::ForceRTC) {
        worker_params.transport_type = ltproto::common::TransportType::RTC;
    }
    else if (host_enable_tcp || client_enable_tcp) {
        worker_params.transport_type = ltproto::common::TransportType::TCP;
    }
    else {
        worker_params.transport_type = ltproto::common::TransportType::RTC;
    }
    worker_params.min_port = min_port;
    worker_params.max_port = max_port;
    worker_params.ignored_nic = settings_->getString("ignored_nic").value_or("");
    worker_params.ioloop = ioloop_.get();
    worker_params.post_task = std::bind(&Service::postTask, this, std::placeholders::_1);
    worker_params.post_delay_task =
        std::bind(&Service::postDelayTask, this, std::placeholders::_1, std::placeholders::_2);
    worker_params.user_defined_relay_server = settings_->getString("relay").value_or("");
    worker_params.msg = msg;
    worker_params.on_create_completed = std::bind(
        &Service::onCreateSessionCompletedThreadSafe, this, std::placeholders::_1,
        std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5);
    worker_params.on_closed =
        std::bind(&Service::onSessionClosedThreadSafe, this, std::placeholders::_1,
                  std::placeholders::_2, std::placeholders::_3, std::placeholders::_4);
    worker_params.on_accepted_connection =
        std::bind(&Service::onAcceptedConnection, this, std::placeholders::_1);
    worker_params.on_connection_status =
        std::bind(&Service::onConnectionStatus, this, std::placeholders::_1);
    worker_params.on_remote_clipboard =
        std::bind(&Service::onRemoteClipboard, this, std::placeholders::_1);
    cached_worker_params_ = worker_params;
    // 3. 校验cookie，通过则直接启动worker，不通过则弹窗让用户确认
    std::string cookie_name = "from_" + std::to_string(msg->client_device_id());
#if 0
    // 暂时不做时间校验
    constexpr int64_t kSecondsPerWeek = int64_t(60) * 60 * 24 * 7;
    const int64_t now = ltlib::utc_now_ms() / 1000; // sqlite的时间戳是UTC+0
    auto update_at = settings_->getUpdateTime(cookie_name);
    if (!update_at.has_value() || now > update_at.value() + kSecondsPerWeek) {
        letUserConfirm(msg->client_device_id());
        return;
    }
#endif // if 0
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
    LOG(INFO) << "LoginDeviceAck: " << ltproto::ErrorCode_Name(ack->err_code());

    // 只有LoginDevice成功后才能告知app，service正常
    if (ack->err_code() == ltproto::ErrorCode::Success) {
        server_logged_ = true;
        auto status = std::make_shared<ltproto::service2app::ServiceStatus>();
        status->set_status(ltproto::ErrorCode::Success);
        sendMessageToApp(ltproto::id(status), status);
    }
}

void Service::onLoginUserAck(std::shared_ptr<google::protobuf::MessageLite> msg) {
    (void)msg;
}

void Service::sendKeepAliveToServer() {
    auto msg = std::make_shared<ltproto::common::KeepAlive>();
    sendMessageToServer(ltproto::id(msg), msg);
    // 5秒发一个心跳包，当前服务端不会检测超时
    // 但是反向代理比如nginx可能设置了proxy_timeout，超过这个时间没有包就会被断链
    postDelayTask(5'000, std::bind(&Service::sendKeepAliveToServer, this));
}

void Service::onCreateSessionCompletedThreadSafe(
    int32_t error_code, int32_t transport_type, int64_t device_id, const std::string& session_name,
    std::shared_ptr<google::protobuf::MessageLite> params) {
    postTask(std::bind(&Service::onCreateSessionCompleted, this, error_code, transport_type,
                       device_id, session_name, params));
}

void Service::onCreateSessionCompleted(int32_t error_code, int32_t transport_type,
                                       int64_t device_id, const std::string& session_name,
                                       std::shared_ptr<google::protobuf::MessageLite> params) {
    (void)session_name;
    auto ack = std::make_shared<ltproto::server::OpenConnectionAck>();
    if (ltproto::common::TransportType_IsValid(transport_type)) {
        ack->set_transport_type(static_cast<ltproto::common::TransportType>(transport_type));
    }
    else {
        LOG(ERR) << "onCreateSessionCompleted with unknown transport type " << transport_type;
        ack->set_transport_type(ltproto::common::TransportType::RTC);
    }
    if (error_code == ltproto::ErrorCode::Success) {
        ack->set_err_code(ltproto::ErrorCode::Success);
        auto streaming_params = ack->mutable_streaming_params();
        auto negotiated_params = std::static_pointer_cast<ltproto::common::StreamingParams>(params);
        streaming_params->CopyFrom(*negotiated_params);
        tcp_client_->send(ltproto::id(ack), ack);
    }
    else {
        if (ltproto::ErrorCode_IsValid(error_code)) {
            ack->set_err_code(static_cast<ltproto::ErrorCode>(error_code));
        }
        else {
            ack->set_err_code(ltproto::ErrorCode::ControlledInitFailed);
        }
        tcp_client_->send(ltproto::id(ack), ack);
        destroySession(session_name);
        tellAppSessionClosed(device_id);
    }
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
    msg->set_version_major(LT_VERSION_MAJOR);
    msg->set_version_minor(LT_VERSION_MINOR);
    msg->set_version_patch(LT_VERSION_PATCH);
    if (allow_control.has_value()) {
        msg->set_allow_control(allow_control.value());
    }
    else {
        msg->set_allow_control(false);
    }
    msg->set_cookie(settings_->getString("device_cookie").value_or(""));
#if defined(LT_WINDOWS)
    msg->set_os_type(ltproto::common::Windows);
#elif defined(LT_LINUX)
    msg->set_os_type(ltproto::common::Linux);
#elif defined(LT_MAC)
    msg->set_os_type(ltproto::common::macOS);
#else
    msg->set_os_type(ltproto::common::UnknownOS);
#endif

    tcp_client_->send(ltproto::id(msg), msg);
}

void Service::loginUser() {}

// TODO: 重新设计这个报告
void Service::reportSessionClosed(WorkerSession::CloseReason close_reason,
                                  const std::string& room_id) {
    auto msg = std::make_shared<ltproto::server::CloseConnection>();
    auto reason = ltproto::server::CloseConnection_Reason_TimeoutClose;
    switch (close_reason) {
    case WorkerSession::CloseReason::ClientClose:
        reason = ltproto::server::CloseConnection_Reason_ClientClose;
        break;
    case WorkerSession::CloseReason::WorkerFailed:
        reason = ltproto::server::CloseConnection_Reason_HostClose;
        break;
    case WorkerSession::CloseReason::Timeout:
        reason = ltproto::server::CloseConnection_Reason_ClientClose;
        break;
    case WorkerSession::CloseReason::UserKick:
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
    case ltype::kOperateConnection:
        onOperateConnection(msg);
        break;
    case ltype::kClipboard:
        onAppClipboard(msg);
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

    // 告知app，service是否已连上server
    auto status = std::make_shared<ltproto::service2app::ServiceStatus>();
    if (server_logged_) {
        status->set_status(ltproto::ErrorCode::Success);
    }
    else {
        status->set_status(ltproto::ErrorCode::ServiceStatusDisconnectedFromServer);
    }
    sendMessageToApp(ltproto::id(status), status);
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
        ack->set_err_code(ltproto::ErrorCode::Unknown);
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
        ack->set_err_code(ltproto::ErrorCode::UserReject); // TODO: reject code
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
        ack->set_err_code(ltproto::ErrorCode::Unknown);
        tcp_client_->send(ltproto::id(ack), ack);
        break;
    }
}

void Service::onOperateConnection(std::shared_ptr<google::protobuf::MessageLite> _msg) {
    if (worker_sessions_.empty()) {
        LOG(WARNING) << "No available connection, can't operate";
        return;
    }
    if (worker_sessions_.size() != 1) {
        LOG(ERR) << "We have more than one accepted connection, something must be wrong";
        return;
    }
    auto session = worker_sessions_.begin();
    auto msg = std::static_pointer_cast<ltproto::service2app::OperateConnection>(_msg);
    for (auto _op : msg->operations()) {
        auto op = static_cast<ltproto::service2app::OperateConnection_Operation>(_op);
        switch (op) {
        case ltproto::service2app::OperateConnection_Operation_EnableGamepad:
            session->second->enableGamepad();
            break;
        case ltproto::service2app::OperateConnection_Operation_DisableGamepad:
            session->second->disableGamepad();
            break;
        case ltproto::service2app::OperateConnection_Operation_EnableKeyboard:
            session->second->enableKeyboard();
            break;
        case ltproto::service2app::OperateConnection_Operation_DisableKeyboard:
            session->second->disableKeyboard();
            break;
        case ltproto::service2app::OperateConnection_Operation_EnableMouse:
            session->second->enableMouse();
            break;
        case ltproto::service2app::OperateConnection_Operation_DisableMouse:
            session->second->disableMouse();
            break;
        case ltproto::service2app::OperateConnection_Operation_EnableAudio:
            session->second->enableAudio();
            break;
        case ltproto::service2app::OperateConnection_Operation_DisableAudio:
            session->second->disableAudio();
            break;
        case ltproto::service2app::OperateConnection_Operation_Kick:
            session->second->close();
            break;
        default:
            LOG(WARNING) << "Unknown operation " << _op;
            break;
        }
    }
}

void Service::onAppClipboard(std::shared_ptr<google::protobuf::MessageLite> msg) {
    for (auto& session : worker_sessions_) {
        if (session.second != nullptr) {
            session.second->onAppClipboard(msg);
        }
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

void Service::onRemoteClipboard(std::shared_ptr<google::protobuf::MessageLite> msg) {
    sendMessageToApp(ltproto::type::kClipboard, msg);
}

} // namespace svc

} // namespace lt