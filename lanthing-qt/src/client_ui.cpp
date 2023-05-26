#include "client_ui.h"

#include <g3log/g3log.hpp>

#include <ltlib/system.h>
#include <ltproto/ltproto.h>
#include <ltproto/server/login_device.pb.h>
#include <ltproto/server/login_device_ack.pb.h>
#include <ltproto/server/request_connection.pb.h>
#include <ltproto/server/request_connection_ack.pb.h>
#include <ltproto/server/allocate_device_id.pb.h>
#include <ltproto/server/allocate_device_id_ack.pb.h>

namespace
{

constexpr ltproto::peer2peer::VideoCodecType kCodecPriority[] = {
    ltproto::peer2peer::VideoCodecType::HEVC,
    ltproto::peer2peer::VideoCodecType::AVC,
};

ltrtc::VideoCodecType to_ltrtc(ltproto::peer2peer::VideoCodecType codec)
{
    switch (codec) {

    case ltproto::peer2peer::AVC:
        return ltrtc::VideoCodecType::H264;
    case ltproto::peer2peer::HEVC:
        return ltrtc::VideoCodecType::H265;
    default:
        return ltrtc::VideoCodecType::Unknown;
    }
}

} // 匿名空间


namespace lt
{

namespace ui
{

ClientUI::~ClientUI()
{
    //
}

bool ClientUI::start(int64_t my_device_id, int64_t peer_device_id)
{
    my_device_id_ = my_device_id;
    peer_device_id_ = peer_device_id;

    ioloop_ = ltlib::IOLoop::create();
    if (ioloop_ == nullptr) {
        return false;
    }
    if (!init_tcp_client()) {
        return false;
    }
    thread_ = ltlib::BlockingThread::create(
        "main_thread", [this](const std::function<void()>& i_am_alive, void*) {
            main_loop(i_am_alive);
        },
        nullptr);
    return true;
}

void ClientUI::wait()
{
    promise_.get_future().get();
}

void ClientUI::main_loop(const std::function<void()>& i_am_alive)
{
    LOG(INFO) << "ClientUI enter main loop";
    ioloop_->run(i_am_alive);
    promise_.set_value();
}

void ClientUI::connect(int64_t device_id)
{
    auto req = std::make_shared<ltproto::server::RequestConnection>();
    req->set_conn_type(ltproto::server::ConnectionType::Control);
    req->set_device_id(device_id);
    //HardDecodability abilities = lt::check_hard_decodability();
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
        std::lock_guard<std::mutex> lock { mutex_ };
        auto result = sessions_.insert({ device_id, nullptr });
        if (!result.second) {
            LOG(WARNING) << "Another task already connected/connecting to device_id:" << device_id;
            return;
        }
    }
    send_message(ltproto::id(req), req);
    try_remove_session_after_10s(device_id);
}

void ClientUI::try_remove_session_after_10s(int64_t device_id)
{
    ioloop_->post_delay(10'000, [device_id, this]() {
        try_remove_session(device_id);
    });
}

void ClientUI::try_remove_session(int64_t device_id)
{
    std::lock_guard<std::mutex> lock { mutex_ };
    auto iter = sessions_.find(device_id);
    if (iter == sessions_.end() || iter->second != nullptr) {
        return;
    } else {
        sessions_.erase(iter);
        LOG(WARNING) << "Remove session(device_id:" << device_id << ") by timeout";
    }
}

void ClientUI::on_client_exited_thread_safe(int64_t device_id)
{
    if (ioloop_->is_not_current_thread()) {
        ioloop_->post(std::bind(&ClientUI::on_client_exited_thread_safe, this, device_id));
        return;
    }
    size_t size;
    {
        std::lock_guard<std::mutex> lock { mutex_ };
        size = sessions_.erase(device_id);
    }
    if (size == 0) {
        LOG(WARNING) << "Try remove ClientSession due to client exited, but the session(" << device_id << ") doesn't exist.";
    } else {
        LOG(INFO) << "Remove session(" << device_id << ") success";
    }
}

bool ClientUI::init_tcp_client()
{
    constexpr uint16_t kPort = 44898;
    constexpr char* kHost = "127.0.0.1";

    ltlib::Client::Params params {};
    params.stype = ltlib::StreamType::TCP;
    params.ioloop = ioloop_.get();
    params.host = kHost;
    params.port = kPort;
    params.is_tls = false;
    params.on_connected = std::bind(&ClientUI::on_server_connected, this);
    params.on_closed = std::bind(&ClientUI::on_server_disconnected, this);
    params.on_reconnecting = std::bind(&ClientUI::on_server_reconnecting, this);
    params.on_message = std::bind(&ClientUI::on_server_message, this, std::placeholders::_1, std::placeholders::_2);
    tcp_client_ = ltlib::Client::create(params);
    return tcp_client_ != nullptr;
}

void ClientUI::send_message(uint32_t type, std::shared_ptr<google::protobuf::MessageLite> msg)
{
    if (ioloop_->is_not_current_thread()) {
        ioloop_->post(std::bind(&ClientUI::send_message, this, type, msg));
        return;
    }
    tcp_client_->send(type, msg);
}

void ClientUI::on_server_connected()
{
    LOG(INFO) << "Connected to server";
    if (my_device_id_ != 0) {
        login_device();
    } else {
        // 前期开发中，device_id写死，不会向服务器申请
        assert(false);
        allocate_device_id();
    }
}

void ClientUI::on_server_disconnected()
{
    LOG(INFO) << "Disconnected from server";
}

void ClientUI::on_server_reconnecting()
{
    LOG(INFO) << "Reconnecting to server...";
}

void ClientUI::on_server_message(uint32_t type, std::shared_ptr<google::protobuf::MessageLite> msg)
{
    LOG(DEBUG) << "On server message, type:" << type;
    namespace ltype = ltproto::type;
    switch (type) {
    case ltype::kLoginDeviceAck:
        handle_login_device_ack(msg);
        break;
    case ltype::kAllocateDeviceIDAck:
        handle_allocate_device_id_ack(msg);
        break;
    case ltype::kRequestConnectionAck:
        handle_request_connection_ack(msg);
        break;
    default:
        LOG(WARNING) << "Unknown server message:" << type;
        break;
    }
}

void ClientUI::login_device()
{
    auto msg = std::make_shared<ltproto::server::LoginDevice>();
    msg->set_device_id(my_device_id_);
    send_message(ltproto::id(msg), msg);
}

void ClientUI::allocate_device_id()
{
    auto msg = std::make_shared<ltproto::server::AllocateDeviceID>();
    send_message(ltproto::id(msg), msg);
}

void ClientUI::handle_allocate_device_id_ack(std::shared_ptr<google::protobuf::MessageLite> msg)
{
    auto ack = std::static_pointer_cast<ltproto::server::AllocateDeviceIDAck>(msg);
    my_device_id_ = ack->device_id();
    //settings_->set_integer("device_id", my_device_id_);
    login_device();
}

void ClientUI::handle_login_device_ack(std::shared_ptr<google::protobuf::MessageLite> _msg)
{
    auto ack = std::static_pointer_cast<ltproto::server::LoginDeviceAck>(_msg);
    if (ack->err_code() != ltproto::server::LoginDeviceAck_ErrCode_Success) {
        LOG(WARNING) << "Login with device id(" << my_device_id_ << ") failed";
        return;
    }
    LOG(INFO) << "LoginDeviceAck: Success";
    // 测试程序，所以登录设备后，直接发起连接。
    connect(peer_device_id_);
}

void ClientUI::handle_request_connection_ack(std::shared_ptr<google::protobuf::MessageLite> _msg)
{
    auto ack = std::static_pointer_cast<ltproto::server::RequestConnectionAck>(_msg);
    if (ack->err_code() != ltproto::server::RequestConnectionAck_ErrCode_Success) {
        LOG(WARNING) << "RequestConnection failed";
        std::lock_guard<std::mutex> lock { mutex_ };
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
    params.on_exited = std::bind(&ClientUI::on_client_exited_thread_safe, this, ack->device_id());
    params.video_codec_type = to_ltrtc(ack->streaming_params().video_codecs().Get(0).codec_type());
    params.width = ack->streaming_params().video_width();
    params.height = ack->streaming_params().video_height();
    params.refresh_rate = ack->streaming_params().screen_refresh_rate();
    params.enable_driver_input = ack->streaming_params().enable_driver_input();
    params.enable_gamepad = ack->streaming_params().enable_gamepad();
    auto session = std::make_shared<ClientSession>(params);
    {
        std::lock_guard<std::mutex> lock { mutex_ };
        auto iter = sessions_.find(ack->device_id());
        if (iter == sessions_.end()) {
            LOG(INFO) << "Received RequestConnectionAck(device_id:" << ack->device_id() << "), but too late";
            return;
        } else if (iter->second != nullptr) {
            LOG(INFO) << "Received RequestConnectionAck(device_id:" << ack->device_id() << "), but another session already started";
            return;
        } else {
            iter->second = session;
            LOG(INFO) << "Received RequestConnectionAck(device_id:" << ack->device_id() << ")";
        }
    }
    if (!session->start()) {
        LOG(INFO) << "Start session(device_id:" << ack->device_id() << ") failed";
        std::lock_guard<std::mutex> lock { mutex_ };
        sessions_.erase(ack->device_id());
    }
}

} // namespace ui

} // namespace lt