#pragma once
#include <cstdint>
#include <memory>

#include <google/protobuf/message_lite.h>

#include <ltlib/io/client.h>
#include <ltlib/io/ioloop.h>
#include <ltlib/io/server.h>
#include <ltlib/threads.h>
#include <transport/transport.h>

#include <graphics/encoder/video_encoder.h>

namespace lt {

namespace svc {

class WorkerProcess;

class WorkerSession {
public:
    enum class CloseReason {
        ClientClose,
        HostClose,
        TimeoutClose,
    };

    struct Params {
        std::string name;
        std::string user_defined_relay_server;
        std::shared_ptr<google::protobuf::MessageLite> msg;
        std::function<void(bool, const std::string&,
                           std::shared_ptr<google::protobuf::MessageLite>)>
            on_create_completed;
        std::function<void(CloseReason, const std::string&, const std::string&)> on_closed;
    };

public:
    static std::shared_ptr<WorkerSession> create(const Params& params);
    ~WorkerSession();

private:
    WorkerSession(
        const std::string& name, const std::string& relay_server,
        std::function<void(bool, const std::string&,
                           std::shared_ptr<google::protobuf::MessageLite>)>
            on_create_completed,
        std::function<void(CloseReason, const std::string&, const std::string&)> on_closed);
    bool init(std::shared_ptr<google::protobuf::MessageLite> msg);
    bool init_rtc_server();
    void create_worker_process(uint32_t client_width, uint32_t client_height,
                               uint32_t client_refresh_rate,
                               std::vector<lt::VideoCodecType> client_codecs);
    void main_loop(const std::function<void()>& i_am_alive);
    void on_closed(CloseReason reason);
    void maybe_on_create_session_completed();
    bool create_video_encoder();

    // 信令
    bool init_signling_client();
    void on_signaling_message_from_net(uint32_t type,
                                       std::shared_ptr<google::protobuf::MessageLite> msg);
    void on_signaling_disconnected();
    void on_signaling_reconnecting();
    void on_signaling_connected();
    void on_signaling_join_room_ack(std::shared_ptr<google::protobuf::MessageLite> msg);
    void on_signaling_message(std::shared_ptr<google::protobuf::MessageLite> msg);
    void on_signaling_message_ack(std::shared_ptr<google::protobuf::MessageLite> msg);
    void dispatch_signaling_message_rtc(std::shared_ptr<google::protobuf::MessageLite> msg);
    void dispatch_signaling_message_core(std::shared_ptr<google::protobuf::MessageLite> msg);

    // worker process
    bool init_pipe_server();
    void on_pipe_accepted(uint32_t fd);
    void on_pipe_disconnected(uint32_t fd);
    void on_pipe_message(uint32_t fd, uint32_t type,
                         std::shared_ptr<google::protobuf::MessageLite> msg);
    void start_working();
    void on_start_working_ack(std::shared_ptr<google::protobuf::MessageLite> msg);
    void send_to_worker(uint32_t type, std::shared_ptr<google::protobuf::MessageLite> msg);
    void on_worker_stoped();
    void on_worker_streaming_params(std::shared_ptr<google::protobuf::MessageLite> msg);
    void send_worker_keep_alive();

    // rtc server
    void on_ltrtc_data(const uint8_t* data, uint32_t size, bool reliable);
    void on_ltrtc_accepted_thread_safe();
    void on_ltrtc_conn_changed();
    void on_ltrtc_failed_thread_safe();
    void on_ltrtc_disconnected_thread_safe();
    void on_ltrtc_signaling_message(const std::string& key, const std::string& value);

    // 数据通道
    void dispatch_dc_message(uint32_t type,
                             const std::shared_ptr<google::protobuf::MessageLite>& msg);
    void on_start_transmission(std::shared_ptr<google::protobuf::MessageLite> msg);
    void on_keep_alive(std::shared_ptr<google::protobuf::MessageLite> msg);
    void on_captured_frame(std::shared_ptr<google::protobuf::MessageLite> msg);
    bool send_message_to_remote_client(uint32_t type,
                                       const std::shared_ptr<google::protobuf::MessageLite>& msg,
                                       bool reliable);

    void update_last_recv_time();
    void check_timeout();

private:
    std::string session_name_;
    std::string user_defined_relay_server_;
    std::unique_ptr<ltlib::IOLoop> ioloop_;
    std::unique_ptr<ltlib::Client> signaling_client_;
    std::unique_ptr<ltlib::BlockingThread> thread_;
    std::unique_ptr<ltlib::TaskThread> task_thread_;
    std::unique_ptr<lt::tp::Server> tp_server_;
    std::unique_ptr<ltlib::Server> pipe_server_;
    std::unique_ptr<lt::VideoEncoder> video_encoder_;
    uint32_t pipe_client_fd_ = std::numeric_limits<uint32_t>::max();
    std::string pipe_name_;
    std::set<uint32_t> worker_registered_msg_;
    std::shared_ptr<WorkerProcess> worker_process_;
    std::string service_id_;
    std::string room_id_;
    std::string auth_token_;
    std::string p2p_username_;
    std::string p2p_password_;
    std::string signaling_addr_;
    uint16_t signaling_port_;
    std::vector<std::string> reflex_servers_;
    std::vector<std::string> relay_servers_;
    std::atomic<bool> client_connected_{false};
    std::function<void(bool, const std::string&, std::shared_ptr<google::protobuf::MessageLite>)>
        on_create_session_completed_;
    std::function<void(CloseReason, const std::string&, const std::string&)> on_closed_;
    std::atomic<int64_t> last_recv_time_us_ = 0;
    bool rtc_closed_ = true;
    bool worker_process_stoped_ = true;
    std::optional<bool> join_signaling_room_success_;
    std::shared_ptr<google::protobuf::MessageLite> negotiated_streaming_params_;
};

} // namespace svc

} // namespace lt