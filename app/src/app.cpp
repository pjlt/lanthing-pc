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

#include "app.h"

#include <filesystem>
#include <iostream>
#include <thread>

#include <ltlib/logging.h>
#include <ltproto/server/allocate_device_id.pb.h>
#include <ltproto/server/allocate_device_id_ack.pb.h>
#include <ltproto/server/login_device.pb.h>
#include <ltproto/server/login_device_ack.pb.h>
#include <ltproto/server/request_connection.pb.h>
#include <ltproto/server/request_connection_ack.pb.h>

#include <ltlib/strings.h>
#include <ltlib/system.h>
#include <ltlib/win_service.h>
#include <ltproto/ltproto.h>

/************************************************************************************
                          +-----------------------+
                          |                       |
                          |                       |
                          |        Server         |
                          |                       |
                      +-> |                       | <--+
                      |   |                       |    |
                      |   +-----------------------+    |
                      |                                |
                      |                                |
             TCP/TLS  |                                |  TCP/TLS
                      |                                |
                      |                                |
                      |                                |
                      |                                |
+-------------------------------+           +---------------------------------+
|                     |         |           |          |                      |
|               +-----+--+      |           |      +---+-----+                |
|               |  App   |      |           |      | Service |                |
|               |        |      |           |      |         |                |
|               +--+-----+      |           |      +----+----+                |
|                  |            |           |           |                     |
|           Launch | IPC        |           |    Launch | IPC                 |
|                  |            |           |           |                     |
|                  v            |           |           v                     |
|                               |           |                                 |
|               +--------+      |           |      +---------+                |
|  Computer A   |Client  |      |           |      | Worker  |   Computer B   |
|               |        |      |           |      |         |                |
|               +--------+      |           |      +---------+                |
|                               |           |                                 |
+-------------------------------+           +---------------------------------+

*************************************************************************************

↓ Every square is a process. The outer rectangle is a computer ↓
+--------------------------------------------------------+
|                                                        |
|                  +-------+                             |
|                  | App   |                             |
|             +----+       +<-+                          |
|      Launch |    +-------+  | IPC                      |
|        &    |               |                          |
|       IPC   v               v                          |
|                                                        |
|       +-------+         +-------+         +-------+    |
|       |Client |         |Service| Launch  |Worker |    |
|       |       |         |       +-------> |       |    |
|       +-------+         +-------+   IPC   +-------+    |
|                                                        |
|                                                        |
+--------------------------------------------------------+

************************************************************************************/

using namespace ltlib::time;

namespace {

const std::string service_name = "Lanthing";
const std::string display_name = "Lanthing Service";

std::string generateAccessToken() {
    constexpr size_t kNumLen = 3;
    constexpr size_t kAlphaLen = 3;
    static const char numbers[] = "0123456789";
    static const char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    std::string str(kNumLen + kAlphaLen, '*');

    for (size_t i = 0; i < kAlphaLen; i++) {
        str[i] = alphabet[rand() % (sizeof(alphabet) - 1)];
    }
    for (size_t i = kAlphaLen; i < kAlphaLen + kNumLen; i++) {
        str[i] = numbers[rand() % (sizeof(numbers) - 1)];
    }
    LOG(DEBUG) << "Generated access token: " << str.c_str();
    return str;
}

} // namespace

namespace lt {

std::unique_ptr<App> lt::App::create() {
    std::unique_ptr<App> app{new App};
    if (!app->init()) {
        return nullptr;
    }
    return app;
}

App::App() {
    //
}

App::~App() {
    {
        std::lock_guard lock{ioloop_mutex_};
        tcp_client_.reset();
        service_manager_.reset();
        client_manager_.reset();
        ioloop_.reset();
    }
    if (!run_as_daemon_) {
        stopService();
    }
}

bool App::init() {
    if (!initSettings()) {
        return false;
    }
    device_id_ = settings_->getInteger("device_id").value_or(0);
    run_as_daemon_ = settings_->getBoolean("daemon").value_or(false);
    auto_refresh_access_token_ = settings_->getBoolean("auto_refresh").value_or(false);
    relay_server_ = settings_->getString("relay").value_or("");

    std::optional<std::string> access_token = settings_->getString("access_token");
    if (access_token.has_value()) {
        access_token_ = access_token.value();
    }
    else {
        access_token_ = generateAccessToken();
        settings_->setString("access_token", access_token_);
    }
    ioloop_ = ltlib::IOLoop::create();
    if (ioloop_ == nullptr) {
        return false;
    }
    if (!initTcpClient()) {
        return false;
    }
    if (!initServiceManager()) {
        return false;
    }
    if (!initClientManager()) {
        return false;
    }
    loadHistoryIDs();
    return true;
}

bool App::initSettings() {
    settings_ = ltlib::Settings::create(ltlib::Settings::Storage::Sqlite);
    return settings_ != nullptr;
}

int App::exec(int argc, char** argv) {
    GUI::Params params{};
    params.connect = std::bind(&App::connect, this, std::placeholders ::_1, std::placeholders::_2);
    params.enable_auto_refresh_access_token =
        std::bind(&App::enableRefreshAccessToken, this, std::placeholders::_1);
    params.enable_run_as_service = std::bind(&App::enableRunAsDaemon, this, std::placeholders::_1);
    params.get_history_device_ids = std::bind(&App::getHistoryDeviceIDs, this);
    params.get_settings = std::bind(&App::getSettings, this);
    params.on_user_confirmed_connection = std::bind(&App::onUserConfirmedConnection, this,
                                                    std::placeholders::_1, std::placeholders::_2);
    params.set_relay_server = std::bind(&App::setRelayServer, this, std::placeholders::_1);

    gui_.init(params, argc, argv);
    thread_ = ltlib::BlockingThread::create(
        "io_thread", [this](const std::function<void()>& i_am_alive) { ioLoop(i_am_alive); });
    return gui_.exec();
}

// 跑在UI线程
void App::connect(int64_t peerDeviceID, const std::string& accessToken) {
    if (peerDeviceID <= 0) {
        LOG(ERR) << "peerDeviceID invalid " << peerDeviceID;
        return;
    }
    postTask([peerDeviceID, accessToken, this]() {
        std::string cookie_name = "cookie_" + std::to_string(peerDeviceID);
        auto cookie = settings_->getString(cookie_name);
        if (!cookie.has_value()) {
            cookie = ltlib::randomStr(24);
        }
        client_manager_->connect(peerDeviceID, accessToken, cookie.value_or(""));
    });
}

std::vector<std::string> App::getHistoryDeviceIDs() const {
    return history_ids_;
}

GUI::Settings App::getSettings() const {
    GUI::Settings settings{};
    settings.auto_refresh_access_token = auto_refresh_access_token_;
    settings.run_as_daemon = run_as_daemon_;
    settings.relay_server = relay_server_;
    return settings;
}

void App::enableRefreshAccessToken(bool enable) {
    auto_refresh_access_token_ = enable;
    settings_->setBoolean("auto_refresh", enable);
}

void App::enableRunAsDaemon(bool enable) {
    run_as_daemon_ = enable;
    settings_->setBoolean("daemon", enable);
}

void App::setRelayServer(const std::string& svr) {
    relay_server_ = svr;
    settings_->setString("relay", svr);
}

void App::onUserConfirmedConnection(int64_t device_id, GUI::ConfirmResult result) {
    service_manager_->onUserConfirmedConnection(device_id, result);
}

void App::ioLoop(const std::function<void()>& i_am_alive) {
    LOG(INFO) << "App enter io loop";
    ioloop_->run(i_am_alive);
    LOG(INFO) << "App exit io loop";
}

void App::createAndStartService() {
#if defined(LT_WINDOWS) && LT_RUN_AS_SERVICE
    std::string path = ltlib::get_program_path<char>();
    std::filesystem::path bin_path(path);
    bin_path /= "lanthing.exe";
    if (!ltlib::ServiceCtrl::createService(service_name, display_name, bin_path.string())) {
        LOGF(ERR, "Create service failed (name:%s, path:%s)", service_name.c_str(),
             bin_path.string().c_str());
        return;
    }
    if (!ltlib::ServiceCtrl::startService(service_name)) {
        LOGF(ERR, "Start service(%s) failed", service_name.c_str());
        return;
    }
    LOGF(INFO, "Start service(%s) success", service_name.c_str());
#endif // if defined(LT_WINDOWS) && LT_RUN_AS_SERVICE
}

void App::stopService() {
#if defined(LT_WINDOWS) && LT_RUN_AS_SERVICE
    ltlib::ServiceCtrl::stopService(service_name);
#endif // if defined(LT_WINDOWS) && LT_RUN_AS_SERVICE
}

void App::loadHistoryIDs() {
    std::string appdata_dir = ltlib::getAppdataPath(/*is_win_service=*/false);
    std::filesystem::path filepath{appdata_dir};
    filepath /= "lanthing";
    filepath /= "historyids";
    std::fstream file{filepath.c_str(), std::ios::in | std::ios::out};
    if (!file.good()) {
        LOGF(WARNING, "Open history ids file(%s) failed", filepath.string().c_str());
        return;
    }
    std::string id;
    uint32_t count = 0;
    while (std::getline(file, id, ';')) {
        if (count >= 20) {
            break;
        }
        history_ids_.push_back(id);
        count++;
        LOG(DEBUG) << "Loaded id " << id;
    }
}

void App::saveHistoryIDs() {
    if (history_ids_.size() > 20) {
        history_ids_.resize(20);
    }
    std::stringstream ss;
    for (const auto& id : history_ids_) {
        ss << id << ';';
    }
    std::string appdata_dir = ltlib::getAppdataPath(/*is_win_service=*/false);
    std::filesystem::path filepath{appdata_dir};
    filepath /= "lanthing";
    filepath /= "historyids";
    std::fstream file{filepath.c_str(), std::ios::out};
    if (!file.good()) {
        LOGF(WARNING, "Open history ids file(%s) failed", filepath.string().c_str());
        return;
    }
    std::string content;
    ss >> content;
    file.write(content.c_str(), content.size());
}

void App::insertNewestHistoryID(const std::string& device_id) {
    if (device_id.empty()) {
        return;
    }
    size_t i = 0;
    while (i < history_ids_.size()) {
        if (history_ids_[i] == device_id) {
            history_ids_.erase(history_ids_.begin() + i);
        }
        else {
            i++;
        }
    }
    history_ids_.insert(history_ids_.begin(), device_id);
}

void App::maybeRefreshAccessToken() {
    if (!auto_refresh_access_token_) {
        return;
    }
    access_token_ = generateAccessToken();
    settings_->setString("access_token", access_token_);
    gui_.setAccessToken(access_token_);
}

void App::onLaunchClientSuccess(int64_t device_id) { // 只将“合法”的device id加入历史列表
    insertNewestHistoryID(std::to_string(device_id));
    saveHistoryIDs();
    maybeRefreshAccessToken();
}

void App::postTask(const std::function<void()>& task) {
    std::lock_guard lock{ioloop_mutex_};
    if (ioloop_) {
        ioloop_->post(task);
    }
}

void App::postDelayTask(int64_t delay_ms, const std::function<void()>& task) {
    std::lock_guard lock{ioloop_mutex_};
    if (ioloop_) {
        ioloop_->postDelay(delay_ms, task);
    }
}

#define MACRO_TO_STRING_HELPER(str) #str
#define MACRO_TO_STRING(str) MACRO_TO_STRING_HELPER(str)
#include <lanthing.cert>
bool App::initTcpClient() {
    ltlib::Client::Params params{};
    params.stype = ltlib::StreamType::TCP;
    params.ioloop = ioloop_.get();
    params.host = MACRO_TO_STRING(LT_SERVER_ADDR);
    params.port = LT_SERVER_APP_PORT;
    params.is_tls = LT_SERVER_USE_SSL;
    params.cert = kLanthingCert;
    params.on_connected = std::bind(&App::onServerConnected, this);
    params.on_closed = std::bind(&App::onServerDisconnected, this);
    params.on_reconnecting = std::bind(&App::onServerReconnecting, this);
    params.on_message =
        std::bind(&App::onServerMessage, this, std::placeholders::_1, std::placeholders::_2);
    tcp_client_ = ltlib::Client::create(params);
    return tcp_client_ != nullptr;
}
#undef MACRO_TO_STRING
#undef MACRO_TO_STRING_HELPER

void App::sendMessage(uint32_t type, std::shared_ptr<google::protobuf::MessageLite> msg) {
    tcp_client_->send(type, msg);
}

void App::sendMessageFromOtherThread(uint32_t type,
                                     std::shared_ptr<google::protobuf::MessageLite> msg) {
    postTask(std::bind(&App::sendMessage, this, type, msg));
}

void App::onServerConnected() {
    LOG(INFO) << "Connected to server";
    if (device_id_ != 0) {
        loginDevice();
    }
    else {
        allocateDeviceID();
    }
    gui_.setLoginStatus(GUI::ErrCode::OK);
}

void App::onServerDisconnected() {
    LOG(ERR) << "Disconnected from server";
    gui_.setLoginStatus(GUI::ErrCode::FALIED);
}

void App::onServerReconnecting() {
    LOG(WARNING) << "Reconnecting to server...";
    gui_.setLoginStatus(GUI::ErrCode::CONNECTING);
}

void App::onServerMessage(uint32_t type, std::shared_ptr<google::protobuf::MessageLite> msg) {
    LOG(DEBUG) << "On server message, type:" << type;
    namespace ltype = ltproto::type;
    switch (type) {
    case ltype::kLoginDeviceAck:
        handleLoginDeviceAck(msg);
        break;
    case ltype::kAllocateDeviceIDAck:
        handleAllocateDeviceIdAck(msg);
        break;
    case ltype::kRequestConnectionAck:
        handleRequestConnectionAck(msg);
        break;
    default:
        LOG(WARNING) << "Unknown server message:" << type;
        break;
    }
}

void App::loginDevice() {
    // NOTE: 运行在ioloop
    auto msg = std::make_shared<ltproto::server::LoginDevice>();
    msg->set_device_id(device_id_);
    sendMessage(ltproto::id(msg), msg);
}

void App::allocateDeviceID() {
    // NOTE: 运行在ioloop
    auto msg = std::make_shared<ltproto::server::AllocateDeviceID>();
    sendMessage(ltproto::id(msg), msg);
}

void App::handleAllocateDeviceIdAck(std::shared_ptr<google::protobuf::MessageLite> msg) {
    auto ack = std::static_pointer_cast<ltproto::server::AllocateDeviceIDAck>(msg);
    device_id_ = ack->device_id();
    settings_->setInteger("device_id", device_id_);
    loginDevice();
}

void App::handleLoginDeviceAck(std::shared_ptr<google::protobuf::MessageLite> _msg) {
    auto ack = std::static_pointer_cast<ltproto::server::LoginDeviceAck>(_msg);
    if (ack->err_code() != ltproto::server::LoginDeviceAck_ErrCode_Success) {
        LOG(ERR) << "Login with device id(" << device_id_ << ") failed";
        return;
    }
    // 登录成功才显示device id
    gui_.setDeviceID(device_id_);
    gui_.setAccessToken(access_token_);
    LOG(INFO) << "LoginDeviceAck: Success";
    createAndStartService();
}

void App::handleRequestConnectionAck(std::shared_ptr<google::protobuf::MessageLite> msg) {
    client_manager_->onRequestConnectionAck(msg);
}

bool App::initServiceManager() {
    ServiceManager::Params params{};
    params.ioloop = ioloop_.get();
    params.on_confirm_connection =
        std::bind(&App::onConfirmConnection, this, std::placeholders::_1);
    params.on_accepted_client = std::bind(&App::onAccpetedClient, this, std::placeholders::_1);
    params.on_client_status = std::bind(&App::onClientStatus, this, std::placeholders::_1);
    params.on_disconnected_client =
        std::bind(&App::onDisconnectedClient, this, std::placeholders::_1);
    service_manager_ = ServiceManager::create(params);
    return service_manager_ != nullptr;
}

void App::onConfirmConnection(int64_t device_id) {
    gui_.onConfirmConnection(device_id);
}

void App::onAccpetedClient(std::shared_ptr<google::protobuf::MessageLite> msg) {
    gui_.onAccptedClient(msg);
}

void App::onDisconnectedClient(int64_t device_id) {
    gui_.onDisconnectedClient(device_id);
}

void App::onClientStatus(std::shared_ptr<google::protobuf::MessageLite> msg) {
    gui_.onClientStatus(msg);
}

bool App::initClientManager() {
    ClientManager::Params params{};
    params.ioloop = ioloop_.get();
    params.on_launch_client_success =
        std::bind(&App::onLaunchClientSuccess, this, std::placeholders::_1);
    params.post_delay_task =
        std::bind(&App::postDelayTask, this, std::placeholders::_1, std::placeholders ::_2);
    params.post_task = std::bind(&App::postTask, this, std::placeholders::_1);
    params.send_message =
        std::bind(&App::sendMessage, this, std::placeholders::_1, std::placeholders ::_2);
    client_manager_ = ClientManager::create(params);
    return client_manager_ != NULL;
}

} // namespace lt
