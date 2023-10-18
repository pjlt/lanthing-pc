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
#include <transport/transport.h>

#include <functional>
#include <memory>
#include <mutex>

#include <google/protobuf/message_lite.h>

#include <ltlib/io/client.h>
#include <ltlib/io/server.h>
#include <ltlib/threads.h>

/*
 * 不管出于性能还是安全考虑，都不应该在局域网以外的场景使用ClientTCP/ServerTCP
 * 提供ClientTCP/ServerTCP仅出于以下目的：
 * 1. 提供一个完整的“不含闭源组件”的Lanthing
 * 2. 提供一个例子，方便依葫芦画瓢替换成自己的传输方案
 */

namespace lt {

namespace tp { // transport

class ClientTCP : public Client {
public:
    struct Params {
        void* user_data;
        VideoCodecType video_codec_type;
        OnData on_data;
        OnVideo on_video;
        OnAudio on_audio;
        OnConnected on_connected;
        OnFailed on_failed;
        OnDisconnected on_disconnected;
        OnSignalingMessage on_signaling_message;
        bool validate() const;
    };

public:
    static std::unique_ptr<ClientTCP> create(const Params& params);
    ~ClientTCP() override;
    bool connect() override;
    void close() override;
    bool sendData(const uint8_t* data, uint32_t size, bool is_reliable) override;
    void onSignalingMessage(const char* key, const char* value) override;

private:
    ClientTCP(const Params& params);
    bool init();
    bool initTcpClient(const std::string& ip, uint16_t port);
    bool isNetworkThread();
    bool isTaskThread();
    void onConnected();
    void onDisconnected();
    void onReconnecting();
    void onMessage(uint32_t type, std::shared_ptr<google::protobuf::MessageLite> msg);
    void netLoop(const std::function<void()>& i_am_alive);
    void onSignalingMessage2(const std::string& key, const std::string& value);
    void handleSigAddress(const std::string& value);
    void invokeInternal(const std::function<void()>& task);
    template <typename ReturnT, typename = std::enable_if<!std::is_void<ReturnT>::value>::type>
    ReturnT invoke(std::function<ReturnT(void)> func) {
        ReturnT ret;
        invokeInternal([func, &ret]() { ret = func(); });
        return ret;
    }

    template <typename ReturnT, typename = std::enable_if<std::is_void<ReturnT>::value>::type>
    void invoke(std::function<ReturnT(void)> task) {
        invokeInternal(task);
    }

private:
    Params params_;
    std::mutex mutex_;
    std::unique_ptr<ltlib::IOLoop> ioloop_;
    std::unique_ptr<ltlib::Client> tcp_client_;
    std::unique_ptr<ltlib::TaskThread> task_thread_;
    std::unique_ptr<ltlib::BlockingThread> net_thread_;
};

class ServerTCP : public Server {
public:
    struct Params {
        void* user_data;
        VideoCodecType video_codec_type;
        OnData on_data;
        OnConnected on_accepted;
        OnFailed on_failed;
        OnDisconnected on_disconnected;
        OnSignalingMessage on_signaling_message;
        bool validate() const;
    };

public:
    static std::unique_ptr<ServerTCP> create(const Params& params);
    ~ServerTCP() override;
    void close() override;
    bool sendData(const uint8_t* data, uint32_t size, bool is_reliable) override;
    bool sendAudio(const AudioData& audio_data) override;
    bool sendVideo(const VideoFrame& frame) override;
    void onSignalingMessage(const char* key, const char* value) override;

private:
    ServerTCP(const Params& params);
    bool init();
    bool initTcpServer();
    bool isNetworkThread();
    bool isTaskThread();
    void onAccepted(uint32_t fd);
    void onDisconnected(uint32_t fd);
    void onMessage(uint32_t fd, uint32_t type, std::shared_ptr<google::protobuf::MessageLite> msg);
    void netLoop(const std::function<void()>& i_am_alive);
    void onSignalingMessage2(const std::string& key, const std::string& value);
    void handleSigConnect();
    bool gatherIP();
    void invokeInternal(const std::function<void()>& task);
    template <typename ReturnT, typename = std::enable_if<!std::is_void<ReturnT>::value>::type>
    ReturnT invoke(std::function<ReturnT(void)> func) {
        ReturnT ret;
        invokeInternal([func, &ret]() { ret = func(); });
        return ret;
    }

    template <typename ReturnT, typename = std::enable_if<std::is_void<ReturnT>::value>::type>
    void invoke(std::function<ReturnT(void)> task) {
        invokeInternal(task);
    }

private:
    Params params_;
    std::mutex mutex_;
    std::unique_ptr<ltlib::IOLoop> ioloop_;
    std::unique_ptr<ltlib::Server> tcp_server_;
    std::unique_ptr<ltlib::TaskThread> task_thread_;
    std::unique_ptr<ltlib::BlockingThread> net_thread_;
    uint32_t client_fd_ = std::numeric_limits<uint32_t>::max();
};

} // namespace tp

} // namespace lt