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

#include <QtWidgets/qmenu.h>
#include <QtWidgets/qsystemtrayicon.h>
#include <QtWidgets/qwidget.h>

using namespace ltlib::time;

namespace {

const std::string service_name = "Lanthing";
const std::string display_name = "Lanthing Service";

constexpr ltproto::peer2peer::VideoCodecType kCodecPriority[] = {
    ltproto::peer2peer::VideoCodecType::HEVC,
    ltproto::peer2peer::VideoCodecType::AVC,
};

lt::VideoCodecType toLtrtc(ltproto::peer2peer::VideoCodecType codec) {
    switch (codec) {
    case ltproto::peer2peer::AVC:
        return lt::VideoCodecType::H264;
    case ltproto::peer2peer::HEVC:
        return lt::VideoCodecType::H265;
    default:
        return lt::VideoCodecType::Unknown;
    }
}

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
        // FIXME: 对文件加锁、解锁、加锁太快会崩，这里的sleep是临时解决方案
        std::this_thread::sleep_for(std::chrono::milliseconds{5});
        settings_->setString("access_token", access_token_);
    }
    ioloop_ = ltlib::IOLoop::create();
    if (ioloop_ == nullptr) {
        return false;
    }
    loadHistoryIDs();
    return true;
}

bool App::initSettings() {
    settings_ = ltlib::Settings::create(ltlib::Settings::Storage::Toml);
    return settings_ != nullptr;
}

int App::exec(int argc, char** argv) {
    QApplication a(argc, argv);

    QIcon icon(":/icons/icons/pc.png");
    QApplication::setWindowIcon(icon);
    QApplication::setQuitOnLastWindowClosed(false);

    MainWindow w(this, nullptr);
    ui_ = &w;

    QSystemTrayIcon sys_tray_icon;
    QMenu* menu = new QMenu();
    QAction* a0 = new QAction("主界面");
    QAction* a1 = new QAction("设置");
    QAction* a2 = new QAction("退出");
    QObject::connect(a0, &QAction::triggered, [&w]() { w.show(); });
    QObject::connect(a1, &QAction::triggered, [&w]() {
        w.switchToSettingPage();
        w.show();
    });
    QObject::connect(a2, &QAction::triggered, []() { QApplication::exit(0); });
    QObject::connect(&sys_tray_icon, &QSystemTrayIcon::activated,
                     [&w](QSystemTrayIcon::ActivationReason reason) {
                         switch (reason) {
                         case QSystemTrayIcon::Unknown:
                             break;
                         case QSystemTrayIcon::Context:
                             break;
                         case QSystemTrayIcon::DoubleClick:
                             w.show();
                             break;
                         case QSystemTrayIcon::Trigger:
                             w.show();
                             break;
                         case QSystemTrayIcon::MiddleClick:
                             break;
                         default:
                             break;
                         }
                     });
    menu->addAction(a0);
    menu->addAction(a1);
    menu->addAction(a2);
    sys_tray_icon.setContextMenu(menu);
    sys_tray_icon.setIcon(icon);

    sys_tray_icon.show();
    w.show();

    // XXX: 暂时先放到这里
    if (!initTcpClient()) {
        return false;
    }

    thread_ = ltlib::BlockingThread::create(
        "io_thread", [this](const std::function<void()>& i_am_alive) { ioLoop(i_am_alive); });

    return a.exec();
}

void App::loginUser() {
    LOG(INFO) << "loginUser not implemented";
}

void App::connect(int64_t peerDeviceID, const std::string& accessToken) {
    int64_t request_id = last_request_id_.fetch_add(1);
    auto req = std::make_shared<ltproto::server::RequestConnection>();
    req->set_request_id(request_id);
    req->set_conn_type(ltproto::server::ConnectionType::Control);
    req->set_device_id(peerDeviceID);
    req->set_access_token(accessToken);
    // HardDecodability abilities = lt::check_hard_decodability();
    bool h264_decodable = true;
    bool h265_decodable = true;
    ltlib::DisplayOutputDesc display_output_desc = ltlib::getDisplayOutputDesc();
    auto params = req->mutable_streaming_params();
    params->set_enable_driver_input(false);
    params->set_enable_gamepad(false);
    params->set_screen_refresh_rate(display_output_desc.frequency);
    params->set_video_width(display_output_desc.width);
    params->set_video_height(display_output_desc.height);
    for (auto codec : kCodecPriority) {
        using Backend = ltproto::peer2peer::StreamingParams::VideoEncodeBackend;
        using CodecType = ltproto::peer2peer::VideoCodecType;
        switch (codec) {
        case ltproto::peer2peer::AVC:
            if (h264_decodable) {
                params->add_video_codecs(CodecType::AVC);
            }
            break;
        case ltproto::peer2peer::HEVC:
            if (h265_decodable) {
                params->add_video_codecs(CodecType::HEVC);
            }
            break;
        default:
            break;
        }
    }
    if (params->video_codecs_size() == 0) {
        LOG(ERR) << "No decodability!";
        return;
    }
    {
        std::lock_guard<std::mutex> lock{session_mutex_};
        auto result = sessions_.insert({request_id, nullptr});
        if (!result.second) {
            LOG(ERR) << "Another task already connected/connecting to device_id:"
                         << peerDeviceID;
            return;
        }
    }
    sendMessageFromOtherThread(ltproto::id(req), req);
    LOGF(INFO, "RequestConnection(device_id:%d, request_id:%d) sent", peerDeviceID, request_id);
    tryRemoveSessionAfter10s(request_id);
}

std::vector<std::string> App::getHistoryDeviceIDs() const {
    return history_ids_;
}

App::Settings App::getSettings() const {
    Settings settings{};
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

void App::ioLoop(const std::function<void()>& i_am_alive) {
    LOG(INFO) << "App enter io loop";
    ioloop_->run(i_am_alive);
    LOG(INFO) << "App exit io loop";
}

void App::tryRemoveSessionAfter10s(int64_t request_id) {
    postDelayTask(10'000, [request_id, this]() { tryRemoveSession(request_id); });
}

void App::tryRemoveSession(int64_t request_id) {
    std::lock_guard<std::mutex> lock{session_mutex_};
    auto iter = sessions_.find(request_id);
    if (iter == sessions_.end() || iter->second != nullptr) {
        return;
    }
    else {
        sessions_.erase(iter);
        LOG(WARNING) << "Remove session(request_id:" << request_id << ") by timeout";
    }
}

void App::onClientExitedThreadSafe(int64_t request_id) {
    postTask([this, request_id]() {
        size_t size;
        {
            std::lock_guard<std::mutex> lock{session_mutex_};
            size = sessions_.erase(request_id);
        }
        if (size == 0) {
            LOG(WARNING)
                << "Try remove ClientSession due to client exited, but the session(request_id:"
                << request_id << ") doesn't exist.";
        }
        else {
            LOG(INFO) << "Remove session(request_id:" << request_id << ") success";
        }
    });
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
    ui_->onLocalAccessToken(access_token_);
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
    ui_->onLoginRet(UiCallback::ErrCode::OK, "backend");
}

void App::onServerDisconnected() {
    LOG(ERR) << "Disconnected from server";
    ui_->onLoginRet(UiCallback::ErrCode::FALIED, "backend");
}

void App::onServerReconnecting() {
    LOG(WARNING) << "Reconnecting to server...";
    ui_->onLoginRet(UiCallback::ErrCode::CONNECTING, "backend");
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
    ui_->onLocalDeviceID(device_id_);
    ui_->onLocalAccessToken(access_token_);
    LOG(INFO) << "LoginDeviceAck: Success";
    createAndStartService();
}

void App::handleRequestConnectionAck(std::shared_ptr<google::protobuf::MessageLite> _msg) {
    auto ack = std::static_pointer_cast<ltproto::server::RequestConnectionAck>(_msg);
    if (ack->err_code() != ltproto::server::RequestConnectionAck_ErrCode_Success) {
        LOGF(WARNING, "RequestConnection(device_id:%d, request_id:%d) failed", ack->device_id(),
             ack->request_id());
        std::lock_guard<std::mutex> lock{session_mutex_};
        sessions_.erase(ack->request_id());
        return;
    }
    ClientSession::Params params{};
    params.client_id = ack->client_id();
    params.room_id = ack->room_id();
    params.auth_token = ack->auth_token();
    params.p2p_username = ack->p2p_username();
    params.p2p_password = ack->p2p_password();
    params.signaling_addr = ack->signaling_addr();
    params.signaling_port = ack->signaling_port();
    params.on_exited = std::bind(&App::onClientExitedThreadSafe, this, ack->request_id());
    params.video_codec_type = toLtrtc(static_cast<ltproto::peer2peer::VideoCodecType>(
        ack->streaming_params().video_codecs().Get(0)));
    params.width = ack->streaming_params().video_width();
    params.height = ack->streaming_params().video_height();
    params.refresh_rate = ack->streaming_params().screen_refresh_rate();
    params.enable_driver_input = ack->streaming_params().enable_driver_input();
    params.enable_gamepad = ack->streaming_params().enable_gamepad();
    params.audio_channels = ack->streaming_params().audio_channels();
    params.audio_freq = ack->streaming_params().audio_sample_rate();
    for (int i = 0; i < ack->reflex_servers_size(); i++) {
        params.reflex_servers.push_back(ack->reflex_servers(i));
    }
    auto session = std::make_shared<ClientSession>(params);
    {
        std::lock_guard<std::mutex> lock{session_mutex_};
        auto iter = sessions_.find(ack->request_id());
        if (iter == sessions_.end()) {
            LOGF(INFO, "Received RequestConnectionAck(device_id:%d, request_id:%d), but too late",
                 ack->device_id(), ack->request_id());
            return;
        }
        else if (iter->second != nullptr) {
            LOGF(INFO,
                 "Received RequestConnectionAck(device_id:%d, request_id:%d), but another session "
                 "already started",
                 ack->device_id(), ack->request_id());
            return;
        }
        else {
            iter->second = session;
            LOGF(INFO, "Received RequestConnectionAck(device_id:, request_id:%d)", ack->device_id(),
                 ack->request_id());
        }
    }
    if (!session->start()) {
        LOGF(INFO, "Start session(device_id:%d, request_id:%d) failed", ack->device_id(),
             ack->request_id());
        std::lock_guard<std::mutex> lock{session_mutex_};
        sessions_.erase(ack->request_id());
    }
    // 只将“合法”的device id加入历史列表
    insertNewestHistoryID(std::to_string(ack->device_id()));
    saveHistoryIDs();
    maybeRefreshAccessToken();
}

} // namespace lt
