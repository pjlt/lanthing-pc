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

#include <functional>
#include <map>
#include <string>

#include <google/protobuf/message_lite.h>

#include <ltlib/io/ioloop.h>
#include <ltlib/io/server.h>

#include <client/client_session.h>

namespace lt {

class ClientManager {
public:
    struct Params {
        ltlib::IOLoop* ioloop;
        uint32_t decode_abilities;
        std::vector<VideoCodecType> codec_priority;
        std::function<void(const std::function<void()>&)> post_task;
        std::function<void(int64_t, const std::function<void()>&)> post_delay_task;
        std::function<void(uint32_t, std::shared_ptr<google::protobuf::MessageLite>)> send_message;
        std::function<void(int64_t)> on_launch_client_success;
        // TODO: 理一下on_connect_failed、on_client_status、on_client_exit几个回调，考虑合并成一个
        std::function<void(int64_t /*device_id*/, int32_t /*error_code*/)> on_connect_failed;
        std::function<void(int32_t)> on_client_status;
        std::function<void(const std::string& /*room_id*/)> close_connection;
    };

public:
    static std::unique_ptr<ClientManager> create(const Params& params);
    void connect(int64_t peerDeviceID, const std::string& accessToken, const std::string& cookie,
                 bool use_tcp);
    void onRequestConnectionAck(std::shared_ptr<google::protobuf::MessageLite> _msg);
    void syncClipboardText(const std::string& text);

private:
    ClientManager(const Params& params);
    bool init(ltlib::IOLoop* ioloop);
    void onPipeAccepted(uint32_t fd);
    void onPipeDisconnected(uint32_t fd);
    void onPipeMessage(uint32_t fd, uint32_t type,
                       std::shared_ptr<google::protobuf::MessageLite> msg);
    void postTask(const std::function<void()>& task);
    void postDelayTask(int64_t, const std::function<void()>& task);
    void sendMessage(uint32_t type, std::shared_ptr<google::protobuf::MessageLite> msg);
    void tryRemoveSessionAfter10s(int64_t request_id);
    void tryRemoveSession(int64_t request_id);
    void onClientExited(int64_t request_id);
    void onClientStatus(std::shared_ptr<google::protobuf::MessageLite> msg);

private:
    const uint32_t decode_abilities_;
    const std::vector<VideoCodecType> codec_priority_;
    std::function<void(const std::function<void()>&)> post_task_;
    std::function<void(int64_t, const std::function<void()>&)> post_delay_task_;
    std::function<void(uint32_t, std::shared_ptr<google::protobuf::MessageLite>)> send_message_;
    std::function<void(int64_t)> on_launch_client_success_;
    std::function<void(int64_t, int32_t)> on_connect_failed_;
    std::function<void(int32_t)> on_client_status_;
    std::function<void(const std::string&)> close_connection_;
    std::atomic<int64_t> last_request_id_{0};
    std::map<int64_t /*request_id*/, std::shared_ptr<ClientSession>> sessions_;
    std::unique_ptr<ltlib::Server> pipe_server_;
};

} // namespace lt