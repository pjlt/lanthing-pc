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
        str[i] = alphabet[rand() % sizeof(alphabet)];
    }
    for (size_t i = kAlphaLen; i < kAlphaLen + kNumLen; i++) {
        str[i] = numbers[rand() % sizeof(numbers)];
    }
    return str;
}

} // namespace

namespace lt {

std::unique_ptr<App> lt::App::create() {
    ::srand(time(nullptr));
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
    stopService();
}

bool App::init() {
    if (!initSettings()) {
        return false;
    }
    std::optional<int64_t> device_id = settings_->get_integer("device_id");
    device_id_ = device_id.value_or(0);
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
    return true;
}

bool App::initSettings() {
    settings_ = ltlib::Settings::create(ltlib::Settings::Storage::Toml);
    return settings_ != nullptr;
}

int App::exec(int argc, char** argv) {
    QApplication a(argc, argv);
    MainWindow w(this, nullptr);
    ui_ = &w;
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
}

} // namespace lt
