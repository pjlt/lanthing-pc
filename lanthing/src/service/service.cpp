#include "service.h"
#include <cassert>
#include <g3log/g3log.hpp>
#include <ltproto/server/open_connection.pb.h>
#include <ltproto/server/open_connection_ack.pb.h>
#include <ltproto/server/close_connection.pb.h>
#include <ltproto/server/login_device.pb.h>
#include <ltproto/server/login_device_ack.pb.h>
#include <ltproto/server/allocate_device_id.pb.h>
#include <ltproto/server/allocate_device_id_ack.pb.h>
#include <ltproto/ltproto.h>
#include <ltlib/strings.h>

namespace lt
{

namespace svc
{

Service::Service() = default;

Service::~Service()
{
    assert(ioloop_->is_not_current_thread());
    ioloop_->stop();
}

bool Service::init()
{
    //if (!init_settings()) {
    //    return false;
    //}
    //std::optional<int64_t> device_id = settings_->get_integer("device_id");
    //device_id_ = device_id.value_or(0);
    device_id_ = 1234567;
    ioloop_ = ltlib::IOLoop::create();
    if (ioloop_ == nullptr) {
        return false;
    }
    if (!init_tcp_client()) {
        return false;
    }
    std::promise<void> promise;
    auto future = promise.get_future();
    thread_ = ltlib::BlockingThread::create(
        "main_thread", [this, &promise](const std::function<void()>& i_am_alive, void*) {
            promise.set_value();
            main_loop(i_am_alive);
        },
        nullptr);
    future.get();
    return true;
}

void Service::uninit()
{
    //
}

bool Service::init_tcp_client()
{
    constexpr uint16_t kSslPort = 43899;
    constexpr uint16_t kNonSslPort = 43898;
    ltlib::Client::Params params {};
    params.stype = ltlib::StreamType::TCP;
    params.ioloop = ioloop_.get();
//#define MACRO_TO_STRING_HELPER(str) #str
//#define MACRO_TO_STRING(str) MACRO_TO_STRING_HELPER(str)
    params.host = "127.0.0.1";
//    params.host = MACRO_TO_STRING(LT_SERVER_ADDR);
//#undef MACRO_TO_STRING
//#undef MACRO_TO_STRING_HELPER
    params.port = kNonSslPort;
    params.is_tls = false;
    params.cert = R"(-----BEGIN CERTIFICATE-----
MIIDpTCCAo2gAwIBAgIUYoMnk9C7H/hbvuHzJ6BAA8ReXRcwDQYJKoZIhvcNAQEL
BQAwYjELMAkGA1UEBhMCQ04xEjAQBgNVBAgMCUd1YW5nZG9uZzERMA8GA1UEBwwI
U2hlbnpoZW4xFTATBgNVBAoMDExhbnRoaW5nIEx0ZDEVMBMGA1UEAwwMbGFudGhp
bmcubmV0MB4XDTIzMDMyMjE1MzgyNVoXDTI0MDMyMTE1MzgyNVowYjELMAkGA1UE
BhMCQ04xEjAQBgNVBAgMCUd1YW5nZG9uZzERMA8GA1UEBwwIU2hlbnpoZW4xFTAT
BgNVBAoMDExhbnRoaW5nIEx0ZDEVMBMGA1UEAwwMbGFudGhpbmcubmV0MIIBIjAN
BgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAlPkFkBTxYN1fNGOfKyoN6jLxu33s
XMA+AF0/EafXugsn7Wv6evHE3wG05zam8Er+8veqwEnfl5TEYg3WFOV/rh+5yfBw
5OQpSclm3jeTaUshsJArVTTMgc/QkGY8rzR7nrRJ4LhBReu1bWYGVR35+x7dIHw/
kyp86rHl4IUOShqJ6EKh+FkvrumHSKn0eISVWYsgEE01Z4PUObhFB0j9BVWC0u7D
sDXOvZlVOxm2Eq9EUHxtoqloDhLhnW03AtFzGMMY7BpD6kVIjw1eMcK2R7r0GgUN
2iBCt4g/BiKGPV8TRiBJKBSPis7+RWkbO281447QbxWgJkWGh1oyDKakuwIDAQAB
o1MwUTAdBgNVHQ4EFgQULUqdsb3Ypvb1ulqg9Os/HzzAlEAwHwYDVR0jBBgwFoAU
LUqdsb3Ypvb1ulqg9Os/HzzAlEAwDwYDVR0TAQH/BAUwAwEB/zANBgkqhkiG9w0B
AQsFAAOCAQEAJ5J5LokF/VGyl4z2IL5flf8wizvLLxf38JJOZ750V3NQrjmWumC4
Kl+cSSmTX+lWaBqH0Mcr7D+5F8C6sVeRIfKs51lzcQTtb36KA79UmBsgl76FqsY9
MlbuuH687XfjX5V6golt6Xux7ox7jY5UCuYfg5fvgM/daX6scmCAVw2rlqG+5vpx
AiJXmUUoJODPzS2BSrYsnKn94ET8ES6A32R5+QR8IU36aclmaZ4hhLlVLIpcaLtH
Ne1cNzfiba5ouL/AA5QjeTCdI/BREGyESu6eOiVtGe26SN+zvfoYz94y1NsMIie0
dr3eXDA0n51BvL+i7ryWJrshwhuT2p4ZLg==
-----END CERTIFICATE-----
)";
    params.on_connected = std::bind(&Service::on_server_connected, this);
    params.on_closed = std::bind(&Service::on_server_disconnected, this);
    params.on_reconnecting = std::bind(&Service::on_server_reconnecting, this);
    params.on_message = std::bind(&Service::on_server_message, this, std::placeholders::_1, std::placeholders::_2);
    tcp_client_ = ltlib::Client::create(params);
    if (tcp_client_ == nullptr) {
        return false;
    }
    return true;
}

void Service::main_loop(const std::function<void()>& i_am_alive)
{
    LOG(INFO) << "Lanthing service enter main loop";
    ioloop_->run(i_am_alive);
}

bool Service::init_settings()
{
    settings_ = Settings::create(Settings::Storage::Toml);
    if (settings_ != nullptr) {
        return true;
    } else {
        return false;
    }
}

void Service::destroy_session(const std::string& session_name)
{
    // worker_sessions_.erase(session_name)会析构WorkerSession内部的PeerConnection
    // 而当前的destroy_session()很可能是PeerConnection信令线程回调上来的
    // 这里选择放到libuv的线程去做
    if (ioloop_->is_not_current_thread()) {
        ioloop_->post(std::bind(&Service::destroy_session, this, session_name));
    } else {
       worker_sessions_.erase(session_name);
    }
}

void Service::on_server_message(uint32_t type, std::shared_ptr<google::protobuf::MessageLite> msg)
{
    dispatch_server_message(type, msg);
}

void Service::dispatch_server_message(uint32_t type, std::shared_ptr<google::protobuf::MessageLite> msg)
{
    namespace ltype = ltproto::type;
    switch (type) {
    case ltype::kLoginDeviceAck:
        on_login_device_ack(msg);
        break;
    case ltype::kLoginUser:
        on_login_user_ack(msg);
        break;
    case ltype::kOpenConnection:
        on_open_connection(msg);
        break;
    default:
        LOG(WARNING) << "Unknown message from server " << type;
        break;
    }
}

void Service::on_server_disconnected()
{
    // 怎么办？
}

void Service::on_server_reconnecting()
{
    LOG(INFO) << "Reconnecting to lanthing server...";
}

void Service::on_server_connected()
{
    LOG(INFO) << "Connected to server";
    if (device_id_ != 0) {
        login_device();
    } else {
        // ID由ClientUI申请好，才能启动Service
        assert(false);
    }
}

void Service::on_open_connection(std::shared_ptr<google::protobuf::MessageLite> _msg)
{
    LOG(INFO) << "Received OpenConnection";
    constexpr size_t kSessionNameLen = 8;
    const std::string session_name = ltlib::random_str(kSessionNameLen);
    {
        std::lock_guard<std::mutex> lock { mutex_ };
        if (!worker_sessions_.empty()) {
            LOG(WARNING) << "Only support one client";
            return;
        } else {
            // 用一个nullptr占位，即使释放锁，其他线程也不会modify这个worker_sessions_
            worker_sessions_[session_name] = nullptr;
        }
    }
    auto msg = std::static_pointer_cast<ltproto::server::OpenConnection>(_msg);
    auto session = WorkerSession::create(
        session_name,
        msg,
        std::bind(&Service::on_create_session_completed_thread_safe, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3),
        std::bind(&Service::on_session_closed_thread_safe, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    auto ack = std::make_shared<ltproto::server::OpenConnectionAck>();
    if (session != nullptr) {
        std::lock_guard<std::mutex> lock { mutex_ };
        worker_sessions_[session_name] = session;
    } else {
        ack->set_err_code(ltproto::server::OpenConnectionAck_ErrCode_Invalid);
        tcp_client_->send(ltproto::id(ack), ack);
        // 删除占位的nullptr
        std::lock_guard<std::mutex> lock { mutex_ };
        worker_sessions_.erase(session_name);
    }
}

void Service::on_login_device_ack(std::shared_ptr<google::protobuf::MessageLite> msg)
{
    auto ack = std::static_pointer_cast<ltproto::server::LoginDeviceAck>(msg);
    LOG(INFO) << "LoginDeviceAck: " << ltproto::server::LoginDeviceAck::ErrCode_Name(ack->err_code());
}

void Service::on_login_user_ack(std::shared_ptr<google::protobuf::MessageLite> msg)
{
}

void Service::on_create_session_completed_thread_safe(bool success, const std::string& session_name, std::shared_ptr<google::protobuf::MessageLite> params)
{
    if (ioloop_->is_not_current_thread()) {
        ioloop_->post(std::bind(&Service::on_create_session_completed_thread_safe, this, success, session_name, params));
        return;
    }
    auto ack = std::make_shared<ltproto::server::OpenConnectionAck>();
    if (success) {
        ack->set_err_code(ltproto::server::OpenConnectionAck_ErrCode_Success);
        auto streaming_params = ack->mutable_streaming_params();
        auto negotiated_params = std::static_pointer_cast<ltproto::peer2peer::StreamingParams>(params);
        streaming_params->CopyFrom(*negotiated_params);
    } else {
        ack->set_err_code(ltproto::server::OpenConnectionAck_ErrCode_Invalid);
    }
    tcp_client_->send(ltproto::id(ack), ack);
}

void Service::on_session_closed_thread_safe(WorkerSession::CloseReason close_reason, const std::string& session_name, const std::string& room_id)
{
    if (ioloop_->is_not_current_thread()) {
        ioloop_->post(std::bind(&Service::on_session_closed_thread_safe, this, close_reason, session_name, room_id));
        return;
    }
    report_session_closed(close_reason, room_id);
    destroy_session(session_name);
}

void Service::send_message_to_server(uint32_t type, std::shared_ptr<google::protobuf::MessageLite> msg)
{
    tcp_client_->send(type, msg);
}

void Service::login_device()
{
    //std::optional<bool> allow_control = settings_->get_boolean("allow_control");
    std::optional<bool> allow_control = true;
    auto msg = std::make_shared<ltproto::server::LoginDevice>();
    msg->set_device_id(device_id_);
    if (allow_control.has_value()) {
        msg->set_allow_control(allow_control.value());
    } else {
        msg->set_allow_control(false);
    }
    tcp_client_->send(ltproto::id(msg), msg);
}

void Service::login_user()
{
}

void Service::report_session_closed(WorkerSession::CloseReason close_reason, const std::string& room_id)
{
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

} // namespace svc

} // namespace lt