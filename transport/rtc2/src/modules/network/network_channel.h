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
#include <mutex>
#include <span>

#include <ltlib/io/ioloop.h>
#include <ltlib/threads.h>

#include <modules/network/address.h>
#include <modules/network/udp_socket.h>
#include <modules/p2p/endpoint_info.h>

namespace rtc2 {

class P2P;
class NetworkChannel {
public:
    struct Params {
        bool is_server;
        Address stun;
        Address relay;
        std::string relay_username;
        std::string relay_password;
        std::string username;
        std::string password;
        std::function<void(int32_t)> on_error;
        std::function<void(const EndpointInfo&)> on_endpoint_info_gathered;
    };

public:
    static std::unique_ptr<NetworkChannel> create(const Params& params);
    bool start();
    void setOnRead(const std::function<void(const uint8_t*, uint32_t, int64_t)>& on_read);
    void setOnConnChanged(const std::function<void(const EndpointInfo&, const EndpointInfo&,
                                                   int64_t)>& on_conn_changed);
    void addRemoteInfo(const EndpointInfo& info);
    int32_t sendPacket(std::vector<std::span<const uint8_t>> spans);
    void post(const std::function<void()>& task);
    void postDelay(uint32_t ms, const std::function<void()>& task);
    std::unique_ptr<UDPSocket> createUDPSocket(const Address& bind_addr);

private:
    NetworkChannel(const Params& params);
    void mainLoop(const std::function<void()>& i_am_alive);
    void onP2PError(int32_t error);
    void onP2PEndpointInfoGathered(const EndpointInfo& info);
    void onP2PRead(const uint8_t* data, uint32_t size, int64_t time_us);
    void onP2PConnchanged(const EndpointInfo& local, const EndpointInfo& remote,
                          int64_t used_time_ms);

private:
    std::mutex mutex_;
    std::shared_ptr<P2P> p2p_;
    std::unique_ptr<ltlib::BlockingThread> thread_;
    std::unique_ptr<ltlib::IOLoop> ioloop_;
    std::function<void(int32_t)> on_error_;
    std::function<void(const EndpointInfo&)> on_endpoint_info_gathered_;
    std::function<void(const uint8_t*, uint32_t, int64_t)> on_read_;
    std::function<void(const EndpointInfo& local, const EndpointInfo& remote, int64_t used_time_ms)>
        on_conn_changed_;
};

} // namespace rtc2