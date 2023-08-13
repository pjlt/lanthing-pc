#include "app.h"

#include <filesystem>
#include <iostream>
#include <thread>

#include <g3log/g3log.hpp>
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

rtc::VideoCodecType toLtrtc(ltproto::peer2peer::VideoCodecType codec) {
    switch (codec) {
    case ltproto::peer2peer::AVC:
        return rtc::VideoCodecType::H264;
    case ltproto::peer2peer::HEVC:
        return rtc::VideoCodecType::H265;
    default:
        return rtc::VideoCodecType::Unknown;
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
    tcp_client_.reset();
    if (ioloop_) {
        ioloop_->stop();
    }
    thread_.reset();
    if (!run_as_daemon_) {
        stopService();
    }
}

bool App::init() {
    if (!initSettings()) {
        return false;
    }
    device_id_ = settings_->get_integer("device_id").value_or(0);
    run_as_daemon_ = settings_->get_boolean("daemon").value_or(false);
    auto_refresh_access_token_ = settings_->get_boolean("auto_refresh").value_or(false);
    relay_server_ = settings_->get_string("relay").value_or("");

    std::optional<std::string> access_token = settings_->get_string("access_token");
    if (access_token.has_value()) {
        access_token_ = access_token.value();
    }
    else {
        access_token_ = generateAccessToken();
        // FIXME: 对文件加锁、解锁、加锁太快会崩，这里的sleep是临时解决方案
        std::this_thread::sleep_for(std::chrono::milliseconds{5});
        settings_->set_string("access_token", access_token_);
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
        "io_thread", [this](const std::function<void()>& i_am_alive, void*) { ioLoop(i_am_alive); },
        nullptr);

    return a.exec();
}

void App::loginUser() {
    LOG(INFO) << "loginUser not implemented";
}

void App::connect(int64_t peerDeviceID, const std::string& accessToken) {
    auto req = std::make_shared<ltproto::server::RequestConnection>();
    req->set_conn_type(ltproto::server::ConnectionType::Control);
    req->set_device_id(peerDeviceID);
    req->set_access_token(accessToken);
    // HardDecodability abilities = lt::check_hard_decodability();
    bool h264_decodable = true;
    bool h265_decodable = true;
    ltlib::DisplayOutputDesc display_output_desc = ltlib::get_display_output_desc();
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
                auto video_codec = params->add_video_codecs();
                video_codec->set_backend(
                    Backend::StreamingParams_VideoEncodeBackend_UnknownVideoEncode);
                video_codec->set_codec_type(CodecType::AVC);
            }
            break;
        case ltproto::peer2peer::HEVC:
            if (h265_decodable) {
                auto video_codec = params->add_video_codecs();
                video_codec->set_backend(
                    Backend::StreamingParams_VideoEncodeBackend_UnknownVideoEncode);
                video_codec->set_codec_type(CodecType::HEVC);
            }
            break;
        default:
            break;
        }
    }
    if (params->video_codecs_size() == 0) {
        LOG(WARNING) << "No decodability!";
        return;
    }
    {
        std::lock_guard<std::mutex> lock{mutex_};
        auto result = sessions_.insert({peerDeviceID, nullptr});
        if (!result.second) {
            LOG(WARNING) << "Another task already connected/connecting to device_id:"
                         << peerDeviceID;
            return;
        }
    }
    sendMessage(ltproto::id(req), req);
    tryRemoveSessionAfter10s(peerDeviceID);
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
    settings_->set_boolean("auto_refresh", enable);
}

void App::enableRunAsDaemon(bool enable) {
    run_as_daemon_ = enable;
    settings_->set_boolean("daemon", enable);
}

void App::setRelayServer(const std::string& svr) {
    relay_server_ = svr;
    settings_->set_string("relay", svr);
}

void App::ioLoop(const std::function<void()>& i_am_alive) {
    LOG(INFO) << "App enter io loop";
    ioloop_->run(i_am_alive);
}

void App::tryRemoveSessionAfter10s(int64_t device_id) {
    ioloop_->post_delay(10'000, [device_id, this]() { tryRemoveSession(device_id); });
}

void App::tryRemoveSession(int64_t device_id) {
    std::lock_guard<std::mutex> lock{mutex_};
    auto iter = sessions_.find(device_id);
    if (iter == sessions_.end() || iter->second != nullptr) {
        return;
    }
    else {
        sessions_.erase(iter);
        LOG(WARNING) << "Remove session(device_id:" << device_id << ") by timeout";
    }
}

void App::onClientExitedThreadSafe(int64_t device_id) {
    if (ioloop_->is_not_current_thread()) {
        ioloop_->post(std::bind(&App::onClientExitedThreadSafe, this, device_id));
        return;
    }
    size_t size;
    {
        std::lock_guard<std::mutex> lock{mutex_};
        size = sessions_.erase(device_id);
    }
    if (size == 0) {
        LOG(WARNING) << "Try remove ClientSession due to client exited, but the session("
                     << device_id << ") doesn't exist.";
    }
    else {
        LOG(INFO) << "Remove session(" << device_id << ") success";
    }
}

void App::createAndStartService() {
#if defined(LT_WINDOWS) && LT_RUN_AS_SERVICE
    std::string path = ltlib::get_program_path<char>();
    std::filesystem::path bin_path(path);
    bin_path /= "lanthing.exe";
    if (!ltlib::ServiceCtrl::createService(service_name, display_name, bin_path.string())) {
        LOGF(WARNING, "Create service failed (name:%s, path:%s)", service_name.c_str(),
             bin_path.string().c_str());
        return;
    }
    if (!ltlib::ServiceCtrl::startService(service_name)) {
        LOGF(WARNING, "Start service(%s) failed", service_name.c_str());
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
    std::string appdata_dir = ltlib::get_appdata_path(/*is_win_service=*/false);
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
    std::string appdata_dir = ltlib::get_appdata_path(/*is_win_service=*/false);
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
    settings_->set_string("access_token", access_token_);
    ui_->onLocalAccessToken(access_token_);
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
    if (ioloop_->is_not_current_thread()) {
        ioloop_->post(std::bind(&App::sendMessage, this, type, msg));
        return;
    }
    tcp_client_->send(type, msg);
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
    LOG(WARNING) << "Disconnected from server";
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
    auto msg = std::make_shared<ltproto::server::LoginDevice>();
    msg->set_device_id(device_id_);
    sendMessage(ltproto::id(msg), msg);
}

void App::allocateDeviceID() {
    auto msg = std::make_shared<ltproto::server::AllocateDeviceID>();
    sendMessage(ltproto::id(msg), msg);
}

void App::handleAllocateDeviceIdAck(std::shared_ptr<google::protobuf::MessageLite> msg) {
    auto ack = std::static_pointer_cast<ltproto::server::AllocateDeviceIDAck>(msg);
    device_id_ = ack->device_id();
    settings_->set_integer("device_id", device_id_);
    loginDevice();
}

void App::handleLoginDeviceAck(std::shared_ptr<google::protobuf::MessageLite> _msg) {
    auto ack = std::static_pointer_cast<ltproto::server::LoginDeviceAck>(_msg);
    if (ack->err_code() != ltproto::server::LoginDeviceAck_ErrCode_Success) {
        LOG(WARNING) << "Login with device id(" << device_id_ << ") failed";
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
        LOG(WARNING) << "RequestConnection failed";
        std::lock_guard<std::mutex> lock{mutex_};
        sessions_.erase(ack->device_id());
        return;
    }
    ClientSession::Params params;
    params.client_id = ack->client_id();
    params.room_id = ack->room_id();
    params.auth_token = ack->auth_token();
    params.p2p_username = ack->p2p_username();
    params.p2p_password = ack->p2p_password();
    params.signaling_addr = ack->signaling_addr();
    params.signaling_port = ack->signaling_port();
    params.on_exited = std::bind(&App::onClientExitedThreadSafe, this, ack->device_id());
    params.video_codec_type = toLtrtc(ack->streaming_params().video_codecs().Get(0).codec_type());
    params.width = ack->streaming_params().video_width();
    params.height = ack->streaming_params().video_height();
    params.refresh_rate = ack->streaming_params().screen_refresh_rate();
    params.enable_driver_input = ack->streaming_params().enable_driver_input();
    params.enable_gamepad = ack->streaming_params().enable_gamepad();
    for (int i = 0; i < ack->reflex_servers_size(); i++) {
        params.reflex_servers.push_back(ack->reflex_servers(i));
    }
    auto session = std::make_shared<ClientSession>(params);
    {
        std::lock_guard<std::mutex> lock{mutex_};
        auto iter = sessions_.find(ack->device_id());
        if (iter == sessions_.end()) {
            LOG(INFO) << "Received RequestConnectionAck(device_id:" << ack->device_id()
                      << "), but too late";
            return;
        }
        else if (iter->second != nullptr) {
            LOG(INFO) << "Received RequestConnectionAck(device_id:" << ack->device_id()
                      << "), but another session already started";
            return;
        }
        else {
            iter->second = session;
            LOG(INFO) << "Received RequestConnectionAck(device_id:" << ack->device_id() << ")";
        }
    }
    if (!session->start()) {
        LOG(INFO) << "Start session(device_id:" << ack->device_id() << ") failed";
        std::lock_guard<std::mutex> lock{mutex_};
        sessions_.erase(ack->device_id());
    }
    // 只将“合法”的device id加入历史列表
    insertNewestHistoryID(std::to_string(ack->device_id()));
    saveHistoryIDs();
    maybeRefreshAccessToken();
}

} // namespace lt
