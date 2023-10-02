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
    bool initRtcServer();
    void createWorkerProcess(uint32_t client_width, uint32_t client_height,
                             uint32_t client_refresh_rate,
                             std::vector<lt::VideoCodecType> client_codecs);
    void mainLoop(const std::function<void()>& i_am_alive);
    void onClosed(CloseReason reason);
    void maybeOnCreateSessionCompleted();
    bool createVideoEncoder();
    void postTask(const std::function<void()>& task);
    void postDelayTask(int64_t delay_ms, const std::function<void()>& task);

    // 信令
    bool initSignlingClient();
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

    // worker process
    bool initPipeServer();
    void onPipeAccepted(uint32_t fd);
    void onPipeDisconnected(uint32_t fd);
    void onPipeMessage(uint32_t fd, uint32_t type,
                       std::shared_ptr<google::protobuf::MessageLite> msg);
    void startWorking();
    void onStartWorkingAck(std::shared_ptr<google::protobuf::MessageLite> msg);
    void sendToWorker(uint32_t type, std::shared_ptr<google::protobuf::MessageLite> msg);
    void sendToWorkerFromOtherThread(uint32_t type,
                                     std::shared_ptr<google::protobuf::MessageLite> msg);
    void onWorkerStoped();
    void onWorkerStreamingParams(std::shared_ptr<google::protobuf::MessageLite> msg);
    void sendWorkerKeepAlive();

    // rtc server
    void onTpData(const uint8_t* data, uint32_t size, bool reliable);
    void onTpAccepted();
    void onTpConnChanged();
    void onTpFailed();
    void onTpDisconnected();
    void onTpSignalingMessage(const std::string& key, const std::string& value);
    void onTpRequestKeyframe();
    void onTpLossRateUpdate(float rate);
    void onTpEesimatedVideoBitreateUpdate(uint32_t bps);

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

    void updateLastRecvTime();
    void checkTimeout();
    void syncTime();
    void getTransportStat();

private:
    std::string session_name_;
    std::string user_defined_relay_server_;
    std::mutex mutex_;
    std::unique_ptr<ltlib::IOLoop> ioloop_;
    std::unique_ptr<ltlib::Client> signaling_client_;
    std::unique_ptr<ltlib::BlockingThread> thread_;
    std::unique_ptr<lt::tp::Server> tp_server_;
    std::unique_ptr<ltlib::Server> pipe_server_;
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
    ltlib::TimeSync time_sync_;
    int64_t rtt_ = 0;
    int64_t time_diff_ = 0;
    float loss_rate_ = .0f;
};

} // namespace svc

} // namespace lt