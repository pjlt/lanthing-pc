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

#pragma once
#include <cstdint>
#include <memory>
#include <mutex>
#include <set>

#include <google/protobuf/message_lite.h>

#include <ltlib/io/client.h>
#include <ltlib/io/ioloop.h>
#include <ltlib/io/server.h>
#include <ltlib/threads.h>
#include <ltlib/time_sync.h>
#include <transport/transport.h>

namespace lt {

namespace svc {

class WorkerProcess;

class WorkerSession : public std::enable_shared_from_this<WorkerSession> {
    struct SpeedEntry {
        int64_t value;
        int64_t timestamp_ms;
    };

public:
    enum class CloseReason {
        ClientClose,
        WorkerFailed,
        Timeout,
        UserKick,
    };

    struct Params {
        std::string name;
        ltlib::IOLoop* ioloop;
        std::string user_defined_relay_server;
        std::shared_ptr<google::protobuf::MessageLite> msg;
        std::function<void(bool, int64_t, const std::string&,
                           std::shared_ptr<google::protobuf::MessageLite>)>
            on_create_completed;
        std::function<void(int64_t, CloseReason, const std::string&, const std::string&)> on_closed;
        std::function<void(const std::function<void()>&)> post_task;
        std::function<void(int64_t, const std::function<void()>&)> post_delay_task;
        std::function<void(std::shared_ptr<google::protobuf::MessageLite>)> on_accepted_connection;
        std::function<void(std::shared_ptr<google::protobuf::MessageLite>)> on_connection_status;
        bool enable_gamepad;
        bool enable_keyboard;
        bool enable_mouse;
        bool force_relay;
        uint16_t min_port;
        uint16_t max_port;
    };

public:
    static std::shared_ptr<WorkerSession> create(const Params& params);
    ~WorkerSession();

    void enableGamepad();
    void disableGamepad();
    void enableMouse();
    void disableMouse();
    void enableKeyboard();
    void disableKeyboard();
    void close();

private:
    WorkerSession(const Params& params);
    bool init(std::shared_ptr<google::protobuf::MessageLite> msg, ltlib::IOLoop* ioloop);
    bool initTransport();
    tp::Server* createTcpServer();
    tp::Server* createRtcServer();
    tp::Server* createRtc2Server();
    void createWorkerProcess(uint32_t client_width, uint32_t client_height,
                             uint32_t client_refresh_rate,
                             std::vector<lt::VideoCodecType> client_codecs);
    void onClosed(CloseReason reason);
    void maybeOnCreateSessionCompleted();
    void postTask(const std::function<void()>& task);
    void postDelayTask(int64_t delay_ms, const std::function<void()>& task);

    // 信令
    bool initSignlingClient(ltlib::IOLoop* ioloop);
    void onSignalingMessageFromNet(uint32_t type,
                                   std::shared_ptr<google::protobuf::MessageLite> msg);
    void onSignalingDisconnected();
    void onSignalingReconnecting();
    void onSignalingConnected();
    void onSignalingJoinRoomAck(std::shared_ptr<google::protobuf::MessageLite> msg);
    void onSignalingMessage(std::shared_ptr<google::protobuf::MessageLite> msg);
    void onSignalingMessageAck(std::shared_ptr<google::protobuf::MessageLite> msg);
    void dispatchSignalingMessageRtc(std::shared_ptr<google::protobuf::MessageLite> msg);
    void dispatchSignalingMessageCore(std::shared_ptr<google::protobuf::MessageLite> msg);
    void sendSigClose();
    void sendToSignalingServer(uint32_t type, std::shared_ptr<google::protobuf::MessageLite> msg);
    void sendKeepAliveToSignalingServer();

    // worker process
    bool initPipeServer(ltlib::IOLoop* ioloop);
    void onPipeAccepted(uint32_t fd);
    void onPipeDisconnected(uint32_t fd);
    void onPipeMessage(uint32_t fd, uint32_t type,
                       std::shared_ptr<google::protobuf::MessageLite> msg);
    void startWorking();
    void onStartWorkingAck(std::shared_ptr<google::protobuf::MessageLite> msg);
    void sendToWorker(uint32_t type, std::shared_ptr<google::protobuf::MessageLite> msg);
    void sendToWorkerFromOtherThread(uint32_t type,
                                     std::shared_ptr<google::protobuf::MessageLite> msg);
    void onKeepAliveAck();
    void onWorkerStreamingParams(std::shared_ptr<google::protobuf::MessageLite> msg);
    void onWorkerFailedFromOtherThread();

    // rtc server
    static void onTpData(void* user_data, const uint8_t* data, uint32_t size, bool reliable);
    static void onTpAccepted(void* user_data, lt::LinkType link_type);
    static void onTpConnChanged(void* user_data);
    static void onTpFailed(void* user_data);
    static void onTpDisconnected(void* user_data);
    static void onTpSignalingMessage(void* user_data, const char* key, const char* value);
    static void onTpRequestKeyframe(void* user_data);
    static void onTpLossRateUpdate(void* user_data, float rate);
    static void onTpEesimatedVideoBitreateUpdate(void* user_data, uint32_t bps);
    static void onTpStat(void* user_data, uint32_t bwe_bps, uint32_t nack);

    // 数据通道
    void dispatchDcMessage(uint32_t type,
                           const std::shared_ptr<google::protobuf::MessageLite>& msg);
    void onStartTransmission(std::shared_ptr<google::protobuf::MessageLite> msg);
    void onKeepAlive(std::shared_ptr<google::protobuf::MessageLite> msg);
    void onCapturedVideo(std::shared_ptr<google::protobuf::MessageLite> msg);
    void onCapturedAudio(std::shared_ptr<google::protobuf::MessageLite> msg);
    void onTimeSync(std::shared_ptr<google::protobuf::MessageLite> msg);
    bool sendMessageToRemoteClient(uint32_t type,
                                   const std::shared_ptr<google::protobuf::MessageLite>& msg,
                                   bool reliable);
    void bypassToClient(uint32_t type, std::shared_ptr<google::protobuf::MessageLite> msg);
    void onChangeStreamingParams(std::shared_ptr<google::protobuf::MessageLite> msg);

    void updateLastRecvTime();
    void checkKeepAliveTimeout();
    void checkAcceptTimeout();
    void syncTime();
    void tellAppAccpetedConnection();
    void sendConnectionStatus(bool repeat, bool gp_hit, bool kb_hit, bool mouse_hit);
    void calcVideoSpeed(int64_t new_frame_bytes);

private:
    std::string session_name_;
    std::function<void(const std::function<void()>&)> post_task_;
    std::function<void(int64_t, const std::function<void()>&)> post_delay_task_;
    std::function<void(std::shared_ptr<google::protobuf::MessageLite>)> on_accepted_connection_;
    std::function<void(std::shared_ptr<google::protobuf::MessageLite>)> on_connection_status_;
    std::string user_defined_relay_server_;
    std::unique_ptr<ltlib::Client> signaling_client_;
    std::unique_ptr<ltlib::BlockingThread> thread_;
    lt::tp::Server* tp_server_ = nullptr;
    std::unique_ptr<ltlib::Server> pipe_server_;
    uint32_t pipe_client_fd_ = std::numeric_limits<uint32_t>::max();
    std::string pipe_name_;
    std::set<uint32_t> worker_registered_msg_;
    std::shared_ptr<WorkerProcess> worker_process_;
    int64_t client_device_id_ = 0;
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
    std::function<void(bool, int64_t, const std::string&,
                       std::shared_ptr<google::protobuf::MessageLite>)>
        on_create_session_completed_;
    std::function<void(int64_t, CloseReason, const std::string&, const std::string&)> on_closed_;
    std::atomic<int64_t> last_recv_time_us_ = 0;
    std::optional<bool> join_signaling_room_success_;
    std::shared_ptr<google::protobuf::MessageLite> negotiated_streaming_params_;
    ltlib::TimeSync time_sync_;
    int64_t rtt_ = 0;
    uint32_t bwe_bps_ = 0;
    int64_t time_diff_ = 0;
    float loss_rate_ = .0f;
    bool is_p2p_ = false;
    bool signaling_keepalive_inited_ = false;
    std::deque<SpeedEntry> video_send_history_;
    int64_t video_send_bps_ = 0;
    bool force_relay_ = false;
    uint16_t min_port_ = 0;
    uint16_t max_port_ = 0;
    bool first_start_working_ack_received_ = false;

    std::atomic<bool> enable_gamepad_;
    std::atomic<bool> enable_keyboard_;
    std::atomic<bool> enable_mouse_;
};

} // namespace svc

} // namespace lt