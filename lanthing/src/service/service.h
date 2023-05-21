#pragma once
#include <ltlib/io/client.h>
#include <ltlib/io/ioloop.h>
#include <ltlib/threads.h>
#include "settings/settings.h"
#include "workers/worker_session.h"

namespace lt
{

namespace svc
{

class Service
{
public:
    Service();
    ~Service();
    bool init();
    void uninit();

private:
    bool init_tcp_client();
    void main_loop(const std::function<void()>& i_am_alive);
    bool init_settings();
    void destroy_session(const std::string& session_name);

    // 服务器
    void on_server_message(uint32_t type, std::shared_ptr<google::protobuf::MessageLite> msg);
    void on_server_disconnected();
    void on_server_reconnecting();
    void on_server_connected();
    void dispatch_server_message(uint32_t type, std::shared_ptr<google::protobuf::MessageLite> msg);
    void send_message_to_server(uint32_t type, std::shared_ptr<google::protobuf::MessageLite> msg);
    void login_device();
    void login_user();
    void allocate_device_id();
    void report_session_closed(WorkerSession::CloseReason close_reason, const std::string& room_id);

    // 消息handler
    void on_open_connection(std::shared_ptr<google::protobuf::MessageLite> msg);
    void on_login_device_ack(std::shared_ptr<google::protobuf::MessageLite> msg);
    void on_login_user_ack(std::shared_ptr<google::protobuf::MessageLite> msg);
    void on_allocate_device_id_ack(std::shared_ptr<google::protobuf::MessageLite> msg);

    void on_create_session_completed_thread_safe(bool success, const std::string& session_name, std::shared_ptr<google::protobuf::MessageLite> msg);
    void on_session_closed_thread_safe(WorkerSession::CloseReason close_reason, const std::string& session_name, const std::string& room_id);

private:
    std::unique_ptr<ltlib::IOLoop> ioloop_;
    std::unique_ptr<ltlib::Client> tcp_client_;
    std::unique_ptr<ltlib::BlockingThread> thread_;
    std::mutex mutex_;
    std::map<std::string, std::shared_ptr<WorkerSession>> worker_sessions_;
    std::unique_ptr<Settings> settings_;
    int64_t device_id_ = 0;
};

} // namespace svc

} // namespace lt