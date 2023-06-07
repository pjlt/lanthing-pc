#include "app.h"

#include <iostream>
#include <thread>

#include <g3log/g3log.hpp>
#include <ltproto/server/allocate_device_id.pb.h>
#include <ltproto/server/allocate_device_id_ack.pb.h>
#include <ltproto/server/login_device.pb.h>
#include <ltproto/server/login_device_ack.pb.h>
#include <ltproto/server/request_connection.pb.h>
#include <ltproto/server/request_connection_ack.pb.h>

#include <ltlib/system.h>
#include <ltproto/ltproto.h>

#include <QtWidgets/qwidget.h>

using namespace ltlib::time;

namespace {

constexpr ltproto::peer2peer::VideoCodecType kCodecPriority[] = {
    ltproto::peer2peer::VideoCodecType::HEVC,
    ltproto::peer2peer::VideoCodecType::AVC,
};

rtc::VideoCodecType to_ltrtc(ltproto::peer2peer::VideoCodecType codec) {
    switch (codec) {
    case ltproto::peer2peer::AVC:
        return rtc::VideoCodecType::H264;
    case ltproto::peer2peer::HEVC:
        return rtc::VideoCodecType::H265;
    default:
        return rtc::VideoCodecType::Unknown;
    }
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
    if (ioloop_) {
        ioloop_->stop();
    }
    thread_.reset();
}

bool App::init() {
    if (!initSettings()) {
        return false;
    }
    std::optional<int64_t> device_id = settings_->get_integer("device_id");
    device_id_ = device_id.value_or(0);
    ioloop_ = ltlib::IOLoop::create();
    if (ioloop_ == nullptr) {
        return false;
    }
    if (!initTcpClient()) {
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
    thread_ = ltlib::BlockingThread::create(
        "io_thread", [this](const std::function<void()>& i_am_alive, void*) { ioLoop(i_am_alive); },
        nullptr);
    return a.exec();
}

void App::loginUser() {
    LOG(INFO) << "loginUser not implemented";
}

void App::connect(int64_t peerDeviceID) {
    auto req = std::make_shared<ltproto::server::RequestConnection>();
    req->set_conn_type(ltproto::server::ConnectionType::Control);
    req->set_device_id(peerDeviceID);
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
                video_codec->set_backend(Backend::StreamingParams_VideoEncodeBackend_Unknown);
                video_codec->set_codec_type(CodecType::AVC);
            }
            break;
        case ltproto::peer2peer::HEVC:
            if (h265_decodable) {
                auto video_codec = params->add_video_codecs();
                video_codec->set_backend(Backend::StreamingParams_VideoEncodeBackend_Unknown);
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

bool App::initTcpClient() {
    constexpr uint16_t kPort = 44898;
    const std::string kHost = "101.43.32.170";

    ltlib::Client::Params params{};
    params.stype = ltlib::StreamType::TCP;
    params.ioloop = ioloop_.get();
    params.host = kHost;
    params.port = kPort;
    params.is_tls = false;
    params.on_connected = std::bind(&App::onServerConnected, this);
    params.on_closed = std::bind(&App::onServerDisconnected, this);
    params.on_reconnecting = std::bind(&App::onServerReconnecting, this);
    params.on_message =
        std::bind(&App::onServerMessage, this, std::placeholders::_1, std::placeholders::_2);
    tcp_client_ = ltlib::Client::create(params);
    return tcp_client_ != nullptr;
}

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
}

void App::onServerDisconnected() {
    LOG(WARNING) << "Disconnected from server";
}

void App::onServerReconnecting() {
    LOG(WARNING) << "Reconnecting to server...";
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
    LOG(INFO) << "LoginDeviceAck: Success";
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
    params.p2p_username = "p2puser";
    params.p2p_password = "p2ppassword";
    params.signaling_addr = ack->signaling_addr();
    params.signaling_port = ack->signaling_port();
    params.on_exited = std::bind(&App::onClientExitedThreadSafe, this, ack->device_id());
    params.video_codec_type = to_ltrtc(ack->streaming_params().video_codecs().Get(0).codec_type());
    params.width = ack->streaming_params().video_width();
    params.height = ack->streaming_params().video_height();
    params.refresh_rate = ack->streaming_params().screen_refresh_rate();
    params.enable_driver_input = ack->streaming_params().enable_driver_input();
    params.enable_gamepad = ack->streaming_params().enable_gamepad();
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
