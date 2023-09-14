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
#include "workers/worker_session.h"

#include <ltlib/io/client.h>
#include <ltlib/io/ioloop.h>
#include <ltlib/settings.h>
#include <ltlib/threads.h>

namespace lt {

namespace svc {

class Service {
public:
    Service();
    ~Service();
    bool init();
    void uninit();

private:
    bool initTcpClient();
    void mainLoop(const std::function<void()>& i_am_alive);
    bool initSettings();
    void destroySession(const std::string& session_name);
    void postTask(const std::function<void()>& task);
    void postDelayTask(int64_t delay_ms, const std::function<void()>& task);

    // 服务器
    void onServerMessage(uint32_t type, std::shared_ptr<google::protobuf::MessageLite> msg);
    void onServerDisconnected();
    void onServerReconnecting();
    void onServerConnected();
    void dispatchServerMessage(uint32_t type, std::shared_ptr<google::protobuf::MessageLite> msg);
    void sendMessageToServer(uint32_t type, std::shared_ptr<google::protobuf::MessageLite> msg);
    void loginDevice();
    void loginUser();
    void reportSessionClosed(WorkerSession::CloseReason close_reason, const std::string& room_id);

    // 消息handler
    void onOpenConnection(std::shared_ptr<google::protobuf::MessageLite> msg);
    void onLoginDeviceAck(std::shared_ptr<google::protobuf::MessageLite> msg);
    void onLoginUserAck(std::shared_ptr<google::protobuf::MessageLite> msg);

    void onCreateSessionCompletedThreadSafe(bool success, const std::string& session_name,
                                            std::shared_ptr<google::protobuf::MessageLite> msg);
    void onCreateSessionCompleted(bool success, const std::string& session_name,
                                  std::shared_ptr<google::protobuf::MessageLite> msg);
    void onSessionClosedThreadSafe(WorkerSession::CloseReason close_reason,
                                   const std::string& session_name, const std::string& room_id);
    void onSessionClosed(WorkerSession::CloseReason close_reason, const std::string& session_name,
                         const std::string& room_id);

private:
    std::unique_ptr<ltlib::IOLoop> ioloop_;
    std::unique_ptr<ltlib::Client> tcp_client_;
    std::unique_ptr<ltlib::BlockingThread> thread_;
    std::mutex mutex_;
    std::map<std::string, std::shared_ptr<WorkerSession>> worker_sessions_;
    std::unique_ptr<ltlib::Settings> settings_;
    int64_t device_id_ = 0;
    // std::string access_token_;
};

} // namespace svc

} // namespace lt