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

#include <ltproto/app/file_chunk.pb.h>
#include <ltproto/app/file_chunk_ack.pb.h>
#include <ltproto/app/pull_file.pb.h>
#include <ltproto/common/clipboard.pb.h>
#include <ltproto/common/keep_alive.pb.h>
#include <ltproto/ltproto.h>
#include <ltproto/server/allocate_device_id.pb.h>
#include <ltproto/server/allocate_device_id_ack.pb.h>
#include <ltproto/server/close_connection.pb.h>
#include <ltproto/server/login_device.pb.h>
#include <ltproto/server/login_device_ack.pb.h>
#include <ltproto/server/new_version.pb.h>
#include <ltproto/server/request_connection.pb.h>
#include <ltproto/server/request_connection_ack.pb.h>
#include <ltproto/service2app/accepted_connection.pb.h>

#include <ltlib/logging.h>
#include <ltlib/pragma_warning.h>
#include <ltlib/strings.h>
#include <ltlib/system.h>
#include <ltlib/versions.h>
#if defined(LT_WINDOWS)
#include "select_gpu.h"
#include <ltlib/win_service.h>
#endif // LT_WINDOWS

#include "check_decode_ability.h"

using namespace ltlib::time;

namespace {

const std::string kServiceName = "Lanthing";
const std::string kDisplayName = "Lanthing Service";

#if defined(LT_WINDOWS)
void logFunc(NbClipLogLevel level, const char* format, ...) {
    constexpr int kMaxMessageSize = 2048;
    char buff[kMaxMessageSize];
    va_list args;
    va_start(args, format);
    int ret = vsnprintf(buff, kMaxMessageSize, format, args);
    if (ret < 0) {
        LOG(WARNING) << "Failed to parse NbClip log message";
        return;
    }
    switch (level) {
    case NbClipLogLevel::Debug:
        LOG(DEBUG) << buff;
        break;
    case NbClipLogLevel::Info:
        LOG(INFO) << buff;
        break;
    case NbClipLogLevel::Warn:
        LOG(WARNING) << buff;
        break;
    case NbClipLogLevel::Error:
        LOG(ERR) << buff;
        break;
    default:
        LOG(WARNING) << "Unknown NbClipLogLevel " << (int)level;
        break;
    }
}
#endif // LT_WINDOWS

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
#if defined(LT_WINDOWS)
    if (nb_clipboard_ != nullptr) {
        destroyNbClipboard(nb_clipboard_);
    }
#endif // LT_WINDOWS
}

bool App::init() {
    if (!initSettings()) {
        return false;
    }
    device_id_ = settings_->getInteger("device_id").value_or(0);
    // run_as_daemon_ = settings_->getBoolean("daemon").value_or(false);
    auto_refresh_access_token_ = settings_->getBoolean("auto_refresh").value_or(false);
    enable_share_clipboard_ = settings_->getBoolean("share_clipboard").value_or(true);
    relay_server_ = settings_->getString("relay").value_or("");
    windowed_fullscreen_ = settings_->getBoolean("windowed_fullscreen");
    min_port_ = static_cast<uint16_t>(settings_->getInteger("min_port").value_or(0));
    max_port_ = static_cast<uint16_t>(settings_->getInteger("max_port").value_or(0));
    status_color_ = settings_->getInteger("status_color").value_or(-1);
    rel_mouse_accel_ = settings_->getInteger("rel_mouse_accel").value_or(0);
    ignored_nic_ = settings_->getString("ignored_nic").value_or("");
    enable_444_ = settings_->getBoolean("enable_444").value_or(false);
    enable_tcp_ = settings_->getBoolean("enable_tcp").value_or(false);
    max_mbps_ = static_cast<uint32_t>(settings_->getInteger("max_mbps").value_or(0));

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
    decode_abilities_ = checkDecodeAbility();
    if (decode_abilities_ == 0) {
        LOG(WARNING) << "This machine has no decode ability!";
    }
    else {
        LOG(INFO) << "Decode ability: " << decode_abilities_;
    }
#if LT_WINDOWS
    selectGPU();
    if (!initServiceManager()) {
        return false;
    }
#endif // LT_WINDOWS
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
    params.enable_share_clipboard =
        std::bind(&App::enableShareClipboard, this, std::placeholders::_1);
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
    params.ignore_version = std::bind(&App::ignoreVersion, this, std::placeholders::_1);
    params.set_port_range =
        std::bind(&App::setPortRange, this, std::placeholders::_1, std::placeholders::_2);
    params.set_status_color = std::bind(&App::setStatusColor, this, std::placeholders::_1);
    params.set_rel_mouse_accel = std::bind(&App::setRelMouseAccel, this, std::placeholders::_1);
    params.set_ignored_nic = std::bind(&App::setIgnoredNIC, this, std::placeholders::_1);
    params.enable_tcp = std::bind(&App::enableTCP, this, std::placeholders::_1);
    params.on_clipboard_text = std::bind(&App::syncClipboardText, this, std::placeholders::_1);
    params.on_clipboard_file =
        std::bind(&App::syncClipboardFile, this, std::placeholders::_1, std::placeholders::_2);
    params.set_max_mbps = std::bind(&App::setMaxMbps, this, std::placeholders::_1);

    gui_.init(params, argc, argv);
    thread_ = ltlib::BlockingThread::create(
        "lt_io_thread", [this](const std::function<void()>& i_am_alive) { ioLoop(i_am_alive); });
    return gui_.exec();
}

// 跑在UI线程
void App::connect(int64_t peerDeviceID, const std::string& accessToken) {
    if (decode_abilities_ == 0) {
        gui_.errorCode(ltproto::ErrorCode::NoDecodeAbility);
        return;
    }
    if (peerDeviceID <= 0) {
        LOG(ERR) << "peerDeviceID invalid " << peerDeviceID;
        return;
    }
    WARNING_DISABLE(4127)
    WARNING_DISABLE(6239)
    if (!LT_ENABLE_SELF_CONNECT && peerDeviceID == device_id_) {
        LOG(INFO) << "Self connect is not allowed";
        gui_.infoMessageBox("Self connect is not allowed");
        return;
    }
    WARNING_ENABLE(4127)
    WARNING_ENABLE(6239)
    postTask([peerDeviceID, accessToken, this]() {
        std::string cookie_name = "to_" + std::to_string(peerDeviceID);
        auto cookie = settings_->getString(cookie_name);
        if (!cookie.has_value()) {
            cookie = ltlib::randomStr(24);
            settings_->setString(cookie_name, cookie.value());
        }
        client_manager_->connect(peerDeviceID, accessToken, cookie.value_or(""), enable_tcp_);
    });
}

std::vector<std::string> App::getHistoryDeviceIDs() const {
    return history_ids_;
}

GUI::Settings App::getSettings() const {
    GUI::Settings settings{};
    settings.auto_refresh_access_token = auto_refresh_access_token_;
    settings.share_clipboard = enable_share_clipboard_;
    settings.run_as_daemon = run_as_daemon_;
    settings.relay_server = relay_server_;
    settings.windowed_fullscreen = windowed_fullscreen_;
    settings.min_port = min_port_;
    settings.max_port = max_port_;
    if (status_color_ >= 0) {
        settings.status_color = static_cast<uint32_t>(status_color_);
    }
    settings.rel_mouse_accel = rel_mouse_accel_;
    settings.ignored_nic = ignored_nic_;
    settings.tcp = enable_tcp_;
    settings.max_mbps = max_mbps_;

    return settings;
}

void App::enableRefreshAccessToken(bool enable) {
    auto_refresh_access_token_ = enable;
    settings_->setBoolean("auto_refresh", enable);
}

void App::enableShareClipboard(bool enable) {
    enable_share_clipboard_ = enable;
    settings_->setBoolean("share_clipboard", enable);
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

void App::ignoreVersion(int64_t version) {
    settings_->setBoolean("ignore_version_" + std::to_string(version), true);
}

void App::setPortRange(uint16_t min_port, uint16_t max_port) {
    min_port_ = min_port;
    max_port_ = max_port;
    settings_->setInteger("min_port", min_port);
    settings_->setInteger("max_port", max_port);
}

void App::setStatusColor(int64_t color) {
    status_color_ = color;
    if (color < 0) {
        settings_->deleteKey("status_color");
    }
    else {
        settings_->setInteger("status_color", color);
    }
}

void App::setRelMouseAccel(int64_t accel) {
    rel_mouse_accel_ = accel;
    if (accel >= 1 && accel <= 30) {
        settings_->setInteger("rel_mouse_accel", accel);
    }
    else {
        settings_->deleteKey("rel_mouse_accel");
    }
}

void App::setIgnoredNIC(const std::string& nic_list) {
    ignored_nic_ = nic_list;
    if (nic_list.empty()) {
        settings_->deleteKey("ignored_nic");
    }
    else {
        settings_->setString("ignored_nic", nic_list);
    }
}

void App::enableTCP(bool enable) {
    enable_tcp_ = enable;
    settings_->setBoolean("enable_tcp", enable);
}

void App::syncClipboardText(const std::string& text) {
    if (!enable_share_clipboard_) {
        return;
    }
    postTask([this, text]() {
        client_manager_->syncClipboardText(text);
        service_manager_->syncClipboardText(text);
    });
}

void App::syncClipboardFile(const std::string& fullpath, uint64_t file_size) {
    (void)fullpath;
    (void)file_size;
    if (!enable_share_clipboard_) {
        return;
    }
#if defined(LT_WINDOWS)
    postTask([this, fullpath, file_size]() {
        if (nb_clipboard_ == nullptr) {
            return;
        }
        auto pos = fullpath.rfind('\\');
        if (pos == std::string::npos) {
            pos = fullpath.rfind('/');
            if (pos == std::string::npos) {
                LOG(ERR) << "syncClipboardFile: fullpath.rfind('\\') and fullpath.rfind('/') "
                            "return npos";
                return;
            }
        }
        std::string filename = fullpath.substr(pos + 1);
        if (filename.empty()) {
            LOG(ERR) << "syncClipboardFile: filename is empty";
            return;
        }
        std::wstring wfullpath = ltlib::utf8To16(fullpath);
        uint32_t file_seq = nb_clipboard_->update_local_file_info(nb_clipboard_, fullpath.c_str(),
                                                                  wfullpath.c_str(), file_size);
        if (file_seq == 0) {
            LOG(WARNING) << "Update local clipboard file info failed";
            return;
        }
        client_manager_->syncClipboardFile(device_id_, file_seq, filename, file_size);
        service_manager_->syncClipboardFile(device_id_, file_seq, filename, file_size);
    });
#endif // LT_WINDOWS
}

void App::setMaxMbps(uint32_t mbps) {
    max_mbps_ = mbps;
    settings_->setInteger("max_mbps", mbps);
}

void App::ioLoop(const std::function<void()>& i_am_alive) {
    LOG(INFO) << "App enter io loop";
    ioloop_->run(i_am_alive);
    LOG(INFO) << "App exit io loop";
}

void App::createAndStartService() {
#if defined(LT_WINDOWS) && LT_RUN_AS_SERVICE
    if (service_started_) {
        return;
    }
    // std::filesystem::path在字符编码上的问题好像挺复杂，先绕过
    std::string bin_path = ltlib::getProgramPath() + "\\lanthing.exe";
    if (!ltlib::ServiceCtrl::createService(kServiceName, kDisplayName, bin_path)) {
        LOGF(ERR, "Create service failed (name:%s, path:%s)", kServiceName.c_str(),
             bin_path.c_str());
        gui_.errorCode(ltproto::ErrorCode::CreateServiceFailed);
        return;
    }
    if (!ltlib::ServiceCtrl::startService(kServiceName)) {
        LOGF(ERR, "Start service(%s) failed", kServiceName.c_str());
        gui_.errorCode(ltproto::ErrorCode::StartServiceFailed);
        return;
    }
    service_started_ = true;
    LOGF(INFO, "Start service(%s) success", kServiceName.c_str());
#endif // if defined(LT_WINDOWS) && LT_RUN_AS_SERVICE
}

void App::stopService() {
#if defined(LT_WINDOWS) && LT_RUN_AS_SERVICE
    ltlib::ServiceCtrl::stopService(kServiceName);
#endif // if defined(LT_WINDOWS) && LT_RUN_AS_SERVICE
}

void App::loadHistoryIDs() {
    std::string appdata_dir = ltlib::getConfigPath(/*is_win_service=*/false);
    std::string filepath = appdata_dir + "\\historyids";
    std::fstream file{filepath.c_str(), std::ios::in | std::ios::out};
    if (!file.good()) {
        LOGF(WARNING, "Open history ids file(%s) failed", filepath.c_str());
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
    std::string appdata_dir = ltlib::getConfigPath(/*is_win_service=*/false);
    std::string filepath = appdata_dir + "\\historyids";
    std::fstream file{filepath.c_str(), std::ios::out};
    if (!file.good()) {
        LOGF(WARNING, "Open history ids file(%s) failed", filepath.c_str());
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

void App::postDelayTask(int64_t delay_ms, const std::function<void()>& task) {
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

#define MACRO_TO_STRING_HELPER(str) #str
#define MACRO_TO_STRING(str) MACRO_TO_STRING_HELPER(str)
#include <trusted-root.cert>
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
#if defined(LT_WINDOWS)
    msg->set_os_type(ltproto::common::Windows);
#elif defined(LT_LINUX)
    msg->set_os_type(ltproto::common::Linux);
#elif defined(LT_MAC)
    msg->set_os_type(ltproto::common::macOS);
#else
    msg->set_os_type(ltproto::common::UnknownOS);
#endif
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
        stoped = stoped_;
    }
    if (stoped) {
        return;
    }
    auto msg = std::make_shared<ltproto::common::KeepAlive>();
    sendMessage(ltproto::id(msg), msg);
    // 5秒发一个心跳包，当前服务端不会检测超时
    // 但是反向代理比如nginx可能设置了proxy_timeout，超过这个时间没有包就会被断链
    postDelayTask(5'000, std::bind(&App::sendKeepAlive, this));
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
    case ltproto::ErrorCode::LoginDeviceInvalidCookie:
        // cookie或ID错误
        LOG(ERR) << "LoginDevice failed: " << ltproto::ErrorCode_Name(ack->err_code()) << " "
                 << ack->err_code();
        if (ack->new_device_id() == 0 || ack->new_cookie().empty()) {
            // 服务器出错，没能成功为我们分配新ID
            gui_.errorCode(ack->err_code());
        }
        else {
            // 记录新ID和cookie，并重新登录
            LOG(WARNING) << "Use the new device " << ack->new_device_id()
                         << " to replace the old one " << device_id_;
            device_id_ = ack->new_device_id();
            settings_->setInteger("device_id", device_id_);
            settings_->setString("device_cookie", ack->new_cookie());
            loginDevice();
        }
        return;
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
    initNbClipboard();
}

void App::handleRequestConnectionAck(std::shared_ptr<google::protobuf::MessageLite> msg) {
    client_manager_->onRequestConnectionAck(msg);
}

void App::handleNewVersion(std::shared_ptr<google::protobuf::MessageLite> _msg) {
    auto msg = std::static_pointer_cast<ltproto::server::NewVersion>(_msg);
    int64_t new_version = ltlib::combineVersion(msg->major(), msg->minor(), msg->patch());
    int64_t my_version =
        ltlib::combineVersion(LT_VERSION_MAJOR, LT_VERSION_MINOR, LT_VERSION_PATCH);
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
    params.on_remote_clipboard = std::bind(&App::onRemoteClipboard, this, std::placeholders::_1);
    params.on_remote_pullfile = std::bind(&App::onRemotePullFile, this, std::placeholders::_1);
    params.on_remote_file_chunk = std::bind(&App::onRemoteFileChunk, this, std::placeholders::_1);
    params.on_remote_file_chunk_ack =
        std::bind(&App::onRemoteFileChunkAck, this, std::placeholders::_1);
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

void App::onRemoteClipboard(std::shared_ptr<google::protobuf::MessageLite> _msg) {
    if (!enable_share_clipboard_) {
        return;
    }
    auto msg = std::static_pointer_cast<ltproto::common::Clipboard>(_msg);
    switch (msg->type()) {
    case ltproto::common::Clipboard_ClipboardType_Text:
        if (msg->text().empty()) {
            LOG(WARNING) << "Received empty clipboard text";
            return;
        }
        gui_.setClipboardText(msg->text());
        break;
    case ltproto::common::Clipboard_ClipboardType_File:
#if defined(LT_WINDOWS)
        if (msg->device_id() == 0 || msg->file_name().empty() || msg->file_size() == 0 ||
            msg->file_seq() == 0) {
            LOG(WARNING) << "Received clipboard file with invliad parameters";
        }
        else if (nb_clipboard_ == nullptr || msg->device_id() == device_id_) {
        }
        else {
            std::wstring wfilename = ltlib::utf8To16(msg->file_name());
            auto success = nb_clipboard_->update_remote_file_info(
                nb_clipboard_, msg->device_id(), msg->file_seq(), msg->file_name().c_str(),
                wfilename.c_str(), msg->file_size());
            if (success == 0) {
                LOG(WARNING) << "Set remote clipboard file '" << msg->file_name()
                             << "' to local failed, maybe we are serving another file";
            }
        }
#endif // LT_WINDOWS
        break;
    default:
        LOG(WARNING) << "Received clipboard message with unkonwn type " << (int)msg->type();
        break;
    }
}

void App::onRemotePullFile(std::shared_ptr<google::protobuf::MessageLite> _msg) {
    if (!enable_share_clipboard_) {
        return;
    }
    (void)_msg;
#if defined(LT_WINDOWS)
    if (nb_clipboard_ == nullptr) {
        return;
    }
    auto msg = std::static_pointer_cast<ltproto::app::PullFile>(_msg);
    if (msg->response_device_id() != device_id_) {
        LOG(WARNING) << "Received PullFile with invalid 'response_device_id'";
        return;
    }
    nb_clipboard_->on_file_pull_request(nb_clipboard_, msg->request_device_id(), msg->file_seq());
#endif // LT_WINDOWS
}

void App::onRemoteFileChunk(std::shared_ptr<google::protobuf::MessageLite> _msg) {
    if (!enable_share_clipboard_) {
        return;
    }
    (void)_msg;
#if defined(LT_WINDOWS)
    if (nb_clipboard_ == nullptr) {
        return;
    }
    auto msg = std::static_pointer_cast<ltproto::app::FileChunk>(_msg);
    if (msg->device_id() != device_id_) {
        LOG(WARNING) << "Received FileChunk with invalid 'device_id'";
        return;
    }
    nb_clipboard_->on_file_chunk(nb_clipboard_, msg->device_id(), msg->file_seq(), msg->chunk_seq(),
                                 reinterpret_cast<const uint8_t*>(msg->data().data()),
                                 static_cast<uint16_t>(msg->data().size()));
#endif // LT_WINDOWS
}

void App::onRemoteFileChunkAck(std::shared_ptr<google::protobuf::MessageLite> _msg) {
    if (!enable_share_clipboard_) {
        return;
    }
    (void)_msg;
#if defined(LT_WINDOWS)
    if (nb_clipboard_ == nullptr) {
        return;
    }
    auto msg = std::static_pointer_cast<ltproto::app::FileChunkAck>(_msg);
    if (msg->device_id() != device_id_) {
        LOG(WARNING) << "Received FileChunkAck with invalid 'device_id'";
        return;
    }
    nb_clipboard_->on_file_chunk_ack(nb_clipboard_, msg->file_seq(), msg->chunk_seq());
#endif // LT_WINDOWS
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
    params.decode_abilities = decode_abilities_;
    if (enable_444_) {
        params.codec_priority = {VideoCodecType::H265_444, VideoCodecType::H264_444,
                                 VideoCodecType::H265_420, VideoCodecType::H264_420,
                                 VideoCodecType::H264_420_SOFT};
    }
    else {
        params.codec_priority = {VideoCodecType::H265_420, VideoCodecType::H264_420,
                                 VideoCodecType::H264_420_SOFT};
    }
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
    params.on_remote_clipboard = std::bind(&App::onRemoteClipboard, this, std::placeholders::_1);
    params.on_remote_pullfile = std::bind(&App::onRemotePullFile, this, std::placeholders::_1);
    params.on_remote_file_chunk = std::bind(&App::onRemoteFileChunk, this, std::placeholders::_1);
    params.on_remote_file_chunk_ack =
        std::bind(&App::onRemoteFileChunkAck, this, std::placeholders::_1);
    client_manager_ = ClientManager::create(params);
    return client_manager_ != nullptr;
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

void App::initNbClipboard() {
#if defined(LT_WINDOWS)
    NbClipboard::Params params{};
    params.userdata = this;
    params.log_print = &logFunc;
    params.send_file_pull_request = [](void* ctx, int64_t peer_device_id, uint32_t file_seq) {
        auto that = reinterpret_cast<App*>(ctx);
        if (peer_device_id == that->device_id_) {
            // 自己连自己时禁用该功能
            LOG(INFO) << "send_file_pull_request peer_device_id == that->device_id_";
            return;
        }
        that->postTask([peer_device_id, file_seq, that]() {
            that->client_manager_->pullFileRequest(that->device_id_, peer_device_id, file_seq);
            that->service_manager_->pullFileRequest(that->device_id_, peer_device_id, file_seq);
        });
    };
    params.send_file_chunk = [](void* ctx, int64_t peer_device_id, uint32_t file_seq,
                                uint32_t chunk_seq, const uint8_t* pdata, uint16_t size) {
        auto that = reinterpret_cast<App*>(ctx);
        if (peer_device_id == that->device_id_) {
            // 自己连自己时禁用该功能
            LOG(INFO) << "send_file_chunk peer_device_id == that->device_id_";
            return;
        }
        std::shared_ptr<uint8_t> sdata(new uint8_t[size]);
        memcpy(sdata.get(), pdata, size);
        that->postTask([peer_device_id, file_seq, chunk_seq, sdata, size, that]() {
            that->client_manager_->sendFileChunk(peer_device_id, file_seq, chunk_seq, sdata.get(),
                                                 size);
            that->service_manager_->sendFileChunk(peer_device_id, file_seq, chunk_seq, sdata.get(),
                                                  size);
        });
    };
    params.send_file_chunk_ack = [](void* ctx, int64_t peer_device_id, uint32_t file_seq,
                                    uint64_t chunk_seq) {
        auto that = reinterpret_cast<App*>(ctx);
        if (peer_device_id == that->device_id_) {
            // 自己连自己时禁用该功能
            LOG(INFO) << "send_file_chunk_ack peer_device_id == that->device_id_";
            return;
        }
        that->postTask([peer_device_id, file_seq, chunk_seq, that]() {
            that->client_manager_->sendFileChunkAck(peer_device_id, file_seq, chunk_seq);
            that->service_manager_->sendFileChunkAck(peer_device_id, file_seq, chunk_seq);
        });
    };
    nb_clipboard_ = createNbClipboard(&params);
    if (nb_clipboard_ == nullptr) {
        LOG(WARNING) << "createNbClipboard failed";
    }
#endif // LT_WINDOWS
}

} // namespace lt
