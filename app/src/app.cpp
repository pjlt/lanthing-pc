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
#include <fstream>
#include <iostream>
#include <thread>

#include <ltlib/logging.h>
#include <ltproto/common/keep_alive.pb.h>
#include <ltproto/server/allocate_device_id.pb.h>
#include <ltproto/server/allocate_device_id_ack.pb.h>
#include <ltproto/server/close_connection.pb.h>
#include <ltproto/server/login_device.pb.h>
#include <ltproto/server/login_device_ack.pb.h>
#include <ltproto/server/new_version.pb.h>
#include <ltproto/server/request_connection.pb.h>
#include <ltproto/server/request_connection_ack.pb.h>
#include <ltproto/service2app/accepted_connection.pb.h>

#include <ltlib/pragma_warning.h>
#include <ltlib/strings.h>
#include <ltlib/system.h>
#if defined(LT_WINDOWS)
#include <ltlib/win_service.h>
#endif // LT_WINDOWS

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
    std::random_device rd;
    rand_engine_ = std::mt19937(rd());
    rand_distrib_ = std::uniform_int_distribution<size_t>();
}

App::~App() {
    {
        std::lock_guard lock{ioloop_mutex_};
        stoped_ = true;
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
    // run_as_daemon_ = settings_->getBoolean("daemon").value_or(false);
    auto_refresh_access_token_ = settings_->getBoolean("auto_refresh").value_or(false);
    relay_server_ = settings_->getString("relay").value_or("");
    windowed_fullscreen_ = settings_->getBoolean("windowed_fullscreen");
    force_relay_ = settings_->getBoolean("force_relay").value_or(false);

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
    stoped_ = false;
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
    params.on_operate_connection =
        std::bind(&App::onOperateConnection, this, std::placeholders::_1);
    params.set_relay_server = std::bind(&App::setRelayServer, this, std::placeholders::_1);
    params.set_fullscreen_mode =
        std::bind(&App::onFullscreenModeChanged, this, std::placeholders::_1);
    params.enable_device_permission =
        std::bind(&App::enableDevicePermission, this, std::placeholders::_1, std::placeholders ::_2,
                  std::placeholders::_3);
    params.delete_trusted_device =
        std::bind(&App::deleteTrustedDevice, this, std::placeholders::_1);
    params.get_trusted_devices = std::bind(&App::getTrustedDevices, this);
    params.force_relay = std::bind(&App::setForceRelay, this, std::placeholders::_1);
    params.ignore_version = std::bind(&App::ignoreVersion, this, std::placeholders::_1);

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
WARNING_DISABLE(4127)
    if (!LT_ENABLE_SELF_CONNECT && peerDeviceID == device_id_) {
        LOG(INFO) << "Self connect is not allowed";
        gui_.infoMessageBox("Self connect is not allowed");
        return;
    }
WARNING_ENABLE(4127)
    postTask([peerDeviceID, accessToken, this]() {
        std::string cookie_name = "to_" + std::to_string(peerDeviceID);
        auto cookie = settings_->getString(cookie_name);
        if (!cookie.has_value()) {
            cookie = ltlib::randomStr(24);
            settings_->setString(cookie_name, cookie.value());
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
    settings.windowed_fullscreen = windowed_fullscreen_;
    settings.force_relay = force_relay_;
    return settings;
}

void App::enableRefreshAccessToken(bool enable) {
    auto_refresh_access_token_ = enable;
    settings_->setBoolean("auto_refresh", enable);
}

void App::enableRunAsDaemon(bool enable) {
    // 屏蔽该功能，不支持无人值守
    (void)enable;
    // run_as_daemon_ = enable;
    // settings_->setBoolean("daemon", enable);
}

void App::setRelayServer(const std::string& svr) {
    relay_server_ = svr;
    settings_->setString("relay", svr);
}

void App::onUserConfirmedConnection(int64_t device_id, GUI::ConfirmResult result) {
    postTask([this, device_id, result]() {
        service_manager_->onUserConfirmedConnection(device_id, result);
    });
}

void App::onOperateConnection(std::shared_ptr<google::protobuf::MessageLite> msg) {
    postTask([this, msg]() { service_manager_->onOperateConnection(msg); });
}

void App::onFullscreenModeChanged(bool is_windowed) {
    postTask([this, is_windowed]() { settings_->setBoolean("windowed_fullscreen", is_windowed); });
}

void App::enableDevicePermission(int64_t device_id, GUI::DeviceType type, bool enable) {
    std::string key;
    switch (type) {
    case GUI::DeviceType::Gamepad:
        key = "enable_gamepad_for_" + std::to_string(device_id);
        break;
    case GUI::DeviceType::Mouse:
        key = "enable_mouse_for_" + std::to_string(device_id);
        break;
    case GUI::DeviceType::Keyboard:
        key = "enable_keyboard_for_" + std::to_string(device_id);
        break;
    default:
        LOG(ERR) << "Unknown DeviceType " << (int)type;
        return;
    }
    settings_->setBoolean(key, enable);
}

void App::deleteTrustedDevice(int64_t device_id) {
    LOG(INFO) << "deleteTrustedDevice " << device_id;
    std::string id_str = std::to_string(device_id);
    std::string key = "from_" + id_str;
    settings_->deleteKey(key);
    key = "enable_mouse_for_" + id_str;
    settings_->deleteKey(key);
    key = "enable_keyboard_for_" + id_str;
    settings_->deleteKey(key);
    key = "enable_gamepad_for_" + id_str;
    settings_->deleteKey(key);
    key = "last_time_from_" + id_str;
    settings_->deleteKey(key);
}

std::vector<GUI::TrustedDevice> App::getTrustedDevices() {
    std::vector<GUI::TrustedDevice> result;
    auto keys = settings_->getKeysStartWith("from_");
    for (auto& key : keys) {
        int64_t device_id = -1;
        int ret = sscanf(key.c_str(), "from_%" PRId64, &device_id);
        if (ret == EOF) {
            LOG(ERR) << "key " << key << " sscanf failed";
            continue;
        }
        auto id_str = std::to_string(device_id);
        std::string key2 = "enable_gamepad_for_" + id_str;
        auto gamepad = settings_->getBoolean(key2);
        key2 = "enable_mouse_for_" + id_str;
        auto mouse = settings_->getBoolean(key2);
        key2 = "enable_keyboard_for_" + id_str;
        auto keyboard = settings_->getBoolean(key2);
        key2 = "last_time_from_" + id_str;
        auto last_time = settings_->getInteger(key2);
        GUI::TrustedDevice device{};
        device.device_id = device_id;
        device.gamepad = gamepad.value_or(true);
        device.keyboard = keyboard.value_or(false);
        device.mouse = mouse.value_or(false);
        device.last_access_time_s = last_time.value_or(0);
        result.push_back(device);
    }
    return result;
}

void App::setForceRelay(bool force) {
    force_relay_ = force;
    settings_->setBoolean("force_relay", force);
}

void App::ignoreVersion(int64_t version) {
    settings_->setBoolean("ignore_version_" + std::to_string(version), true);
}

void App::ioLoop(const std::function<void()>& i_am_alive) {
    LOG(INFO) << "App enter io loop";
    ioloop_->run(i_am_alive);
    LOG(INFO) << "App exit io loop";
}

void App::createAndStartService() {
#if defined(LT_WINDOWS) && LT_RUN_AS_SERVICE
    std::string path = ltlib::getProgramPath();
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

void App::onConnectFailed(int64_t device_id, int32_t error_code) {
    (void)device_id;
    gui_.errorCode(error_code);
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
#include <ISRG-Root.cert>
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
    if (!signaling_keepalive_inited_) {
        signaling_keepalive_inited_ = false;
        sendKeepAlive();
    }
}

void App::onServerDisconnected() {
    LOG(ERR) << "Disconnected from server";
    gui_.setLoginStatus(GUI::LoginStatus::Disconnected);
}

void App::onServerReconnecting() {
    LOG(WARNING) << "Reconnecting to server...";
    gui_.setLoginStatus(GUI::LoginStatus::Connecting);
}

void App::onServerMessage(uint32_t type, std::shared_ptr<google::protobuf::MessageLite> msg) {
    LOG(DEBUG) << "On server message, type:" << type;
    namespace ltype = ltproto::type;
    switch (type) {
    case ltype::kKeepAliveAck:
        // do nothing
        break;
    case ltype::kLoginDeviceAck:
        handleLoginDeviceAck(msg);
        break;
    case ltype::kAllocateDeviceIDAck:
        handleAllocateDeviceIdAck(msg);
        break;
    case ltype::kRequestConnectionAck:
        handleRequestConnectionAck(msg);
        break;
    case ltype::kNewVersion:
        handleNewVersion(msg);
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
    msg->set_version_major(LT_VERSION_MAJOR);
    msg->set_version_minor(LT_VERSION_MINOR);
    msg->set_version_patch(LT_VERSION_PATCH);
    msg->set_cookie(settings_->getString("device_cookie").value_or(""));
    sendMessage(ltproto::id(msg), msg);
}

void App::allocateDeviceID() {
    // NOTE: 运行在ioloop
    auto msg = std::make_shared<ltproto::server::AllocateDeviceID>();
    sendMessage(ltproto::id(msg), msg);
}

void App::sendKeepAlive() {
    bool stoped = false;
    {
        std::lock_guard lk{ioloop_mutex_};
        stoped = stoped;
    }
    if (stoped) {
        return;
    }
    auto msg = std::make_shared<ltproto::common::KeepAlive>();
    sendMessage(ltproto::id(msg), msg);
    // 10秒发一个心跳包，当前服务端不会检测超时
    // 但是反向代理比如nginx可能设置了proxy_timeout，超过这个时间没有包就会被断链
    postDelayTask(10'000, std::bind(&App::sendKeepAlive, this));
}

void App::handleAllocateDeviceIdAck(std::shared_ptr<google::protobuf::MessageLite> msg) {
    auto ack = std::static_pointer_cast<ltproto::server::AllocateDeviceIDAck>(msg);
    device_id_ = ack->device_id();
    settings_->setInteger("device_id", device_id_);
    if (!ack->cookie().empty()) {
        settings_->setString("device_cookie", ack->cookie());
    }
    loginDevice();
}

void App::handleLoginDeviceAck(std::shared_ptr<google::protobuf::MessageLite> _msg) {
    auto ack = std::static_pointer_cast<ltproto::server::LoginDeviceAck>(_msg);
    switch (ack->err_code()) {
    case ltproto::ErrorCode::Success:
        LOG(INFO) << "LoginDeviceAck: Success";
        break;
    case ltproto::ErrorCode::LoginDeviceInvalidStatus:
        // 服务端产生无法解决的错误，客户端也无法处理，直接返回
        LOG(ERR) << "LoginDevice failed, LoginDeviceInvalidStatus " << ack->err_code();
        gui_.errorCode(ack->err_code());
        return;
    case ltproto::ErrorCode::LoginDeviceInvalidID:
        // ID错误，如果服务端下发了新ID，则使用新ID继续流程，否则当成错误直接返回
        LOG(ERR) << "LoginDevice failed, LoginDeviceInvalidID";
        if (ack->new_device_id() != 0) {
            LOG(WARNING) << "Use the new device " << ack->new_device_id()
                         << " to replace the old one " << device_id_;
            device_id_ = ack->new_device_id();
            settings_->setInteger("device_id", device_id_);
        }
        else {
            gui_.errorCode(ack->err_code());
            return;
        }
        break;
    case ltproto::ErrorCode::LoginDeviceInvalidCookie:
        // cookie错误，可能在settings.db手写了别人的ID。如果服务端下发了新ID则使用新ID，否则当成错误返回
        LOG(ERR) << "LoginDevice failed, LoginDeviceInvalidCookie " << ack->err_code();
        if (ack->new_device_id() != 0) {
            LOG(WARNING) << "Use the new device " << ack->new_device_id()
                         << " to replace the old one " << device_id_;
            device_id_ = ack->new_device_id();
            settings_->setInteger("device_id", device_id_);
        }
        else {
            gui_.errorCode(ack->err_code());
            return;
        }
        break;
    default:
        // 未知错误
        LOG(ERR) << "LoginDevice failed, unknown error: " << static_cast<int>(ack->err_code());
        gui_.errorCode(ack->err_code());
        return;
    }
    // 服务端随时可能下发新cookie
    if (!ack->new_cookie().empty()) {
        // NOTE: 这个cookie不能在log打出来！！！
        LOG(INFO) << "Update device cookie";
        settings_->setString("device_cookie", ack->new_cookie());
    }
    // 在UI上显示ID，并启动被控服务
    gui_.setDeviceID(device_id_);
    gui_.setAccessToken(access_token_);
    gui_.setLoginStatus(GUI::LoginStatus::Connected);
    createAndStartService();
}

void App::handleRequestConnectionAck(std::shared_ptr<google::protobuf::MessageLite> msg) {
    client_manager_->onRequestConnectionAck(msg);
}

void App::handleNewVersion(std::shared_ptr<google::protobuf::MessageLite> _msg) {
    auto msg = std::static_pointer_cast<ltproto::server::NewVersion>(_msg);
    int64_t new_version = msg->major() * 1'000'000 + msg->minor() * 1'000 + msg->patch();
    int64_t my_version = LT_VERSION_MAJOR * 1'000'000 + LT_VERSION_MINOR * 1'000 + LT_VERSION_PATCH;
    if (my_version == new_version) {
        return;
    }
    else if (my_version > new_version) {
        LOG(WARNING) << "New version " << new_version << " smaller than current version "
                     << my_version << ",  bug or testing?";
        return;
    }
    auto ignore = settings_->getBoolean("ignore_version_" + std::to_string(new_version));
    if (ignore.has_value()) {
        LOG(INFO) << "The user has previously chosen not to upgrade to this version "
                  << new_version;
        return;
    }
    gui_.onNewVersion(msg);
}

bool App::initServiceManager() {
    ServiceManager::Params params{};
    params.ioloop = ioloop_.get();
    params.on_confirm_connection =
        std::bind(&App::onConfirmConnection, this, std::placeholders::_1);
    params.on_accepted_connection =
        std::bind(&App::onAccpetedConnection, this, std::placeholders::_1);
    params.on_connection_status = std::bind(&App::onConnectionStatus, this, std::placeholders::_1);
    params.on_disconnected_connection =
        std::bind(&App::onDisconnectedConnection, this, std::placeholders::_1);
    params.on_service_status = std::bind(&App::onServiceStatus, this, std::placeholders::_1);
    service_manager_ = ServiceManager::create(params);
    return service_manager_ != nullptr;
}

void App::onConfirmConnection(int64_t device_id) {
    gui_.onConfirmConnection(device_id);
}

void App::onAccpetedConnection(std::shared_ptr<google::protobuf::MessageLite> _msg) {
    auto msg = std::static_pointer_cast<ltproto::service2app::AcceptedConnection>(_msg);
    std::string key = "last_time_from_" + std::to_string(msg->device_id());
    settings_->setInteger(key, ltlib::utc_now_ms() / 1000);
    gui_.onAccptedConnection(_msg);
}

void App::onDisconnectedConnection(int64_t device_id) {
    gui_.onDisconnectedConnection(device_id);
}

void App::onConnectionStatus(std::shared_ptr<google::protobuf::MessageLite> msg) {
    gui_.onConnectionStatus(msg);
}

void App::onServiceStatus(ServiceManager::ServiceStatus status) {
    switch (status) {
    case lt::ServiceManager::ServiceStatus::Up:
        gui_.onServiceStatus(GUI::ServiceStatus::Up);
        break;
    case lt::ServiceManager::ServiceStatus::Down:
        gui_.onServiceStatus(GUI::ServiceStatus::Down);
        break;
    default:
        break;
    }
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
    params.on_connect_failed =
        std::bind(&App::onConnectFailed, this, std::placeholders::_1, std::placeholders ::_2);
    params.on_client_status = std::bind(&App::onClientStatus, this, std::placeholders::_1);
    params.close_connection = std::bind(&App::closeConnectionByRoomID, this, std::placeholders::_1);
    client_manager_ = ClientManager::create(params);
    return client_manager_ != NULL;
}

void App::onClientStatus(int32_t err_code) {
    gui_.errorCode(err_code);
}

void App::closeConnectionByRoomID(const std::string& room_id) {
    auto msg = std::make_shared<ltproto::server::CloseConnection>();
    // 原因乱填的，涉及断链的逻辑都得重新理一理
    msg->set_reason(ltproto::server::CloseConnection_Reason_ClientClose);
    msg->set_room_id(room_id);
    sendMessage(ltproto::id(msg), msg);
}

size_t App::rand() {
    return rand_distrib_(rand_engine_);
}

std::string App::generateAccessToken() {
    constexpr size_t kNumLen = 3;
    constexpr size_t kAlphaLen = 3;
    static const char numbers[] = "0123456789";
    static const char alphabet[] = "abcdefghijklmnopqrstuvwxyz";
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

} // namespace lt
