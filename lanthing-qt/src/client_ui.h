#pragma once
#include <cstdint>
#include <string>
#include <functional>
#include <memory>
#include <future>

#include <google/protobuf/message_lite.h>

#include <ltlib/io/client.h>
#include <ltlib/io/ioloop.h>
#include <ltlib/threads.h>

#include "client_session.h"

namespace lt
{

namespace ui
{

// 这个名字其实很不好
class ClientUI
{
public:
    ClientUI() = default;
    ~ClientUI();
    bool start(int64_t my_device_id, int64_t peer_device_id);
    // 由于没人调ioloop_->stop()，这里会永远wait下去
    void wait();

private:
    void main_loop(const std::function<void()>& i_am_alive);
    void connect(int64_t device_id);
    void try_remove_session_after_10s(int64_t device_id);
    void try_remove_session(int64_t device_id);
    void on_client_exited_thread_safe(int64_t device_id);

    // tcp client
    bool init_tcp_client();
    void send_message(uint32_t type, std::shared_ptr<google::protobuf::MessageLite> msg);
    void on_server_connected();
    void on_server_disconnected();
    void on_server_reconnecting();
    void on_server_message(uint32_t type, std::shared_ptr<google::protobuf::MessageLite> msg);

    // message handler
    void handle_login_device_ack(std::shared_ptr<google::protobuf::MessageLite> msg);
    void handle_request_connection_ack(std::shared_ptr<google::protobuf::MessageLite> msg);

private:
    int64_t my_device_id_;
    int64_t peer_device_id_;

private:
    std::unique_ptr<ltlib::IOLoop> ioloop_;
    std::unique_ptr<ltlib::Client> tcp_client_;
    std::map<int64_t /*to_device*/, std::shared_ptr<ClientSession>> sessions_;
    std::unique_ptr<ltlib::BlockingThread> thread_;
    std::mutex mutex_;
    std::promise<void> promise_;
};

} // namespace ui

} // namespace lt