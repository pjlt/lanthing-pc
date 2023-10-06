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
#include <memory>
#include <span>
#include <string>

#include <modules/p2p/endpoint.h>

#include <modules/network/address.h>
#include <modules/p2p/lan_endpoint.h>
#include <modules/p2p/relay_endpoint.h>
#include <modules/p2p/wan_endpoint.h>

namespace rtc2 {

class NetworkChannel;

class P2P : public std::enable_shared_from_this<P2P> {
public:
    struct Params {
        bool is_server = false;
        NetworkChannel* network_channel;
        Address stun;
        Address relay;
        std::string relay_username;
        std::string relay_password;
        std::string username;
        std::string password;
        std::function<void(int32_t)> on_error;
        std::function<void(const EndpointInfo&)> on_endpoint_info_gathered;
        std::function<void(const uint8_t*, uint32_t, int64_t)> on_read;
        std::function<void(const EndpointInfo& local, const EndpointInfo& remote,
                           int64_t used_time_ms)>
            on_conn_changed;
    };

public:
    P2P(const Params& params);
    ~P2P();

    int32_t send(std::vector<std::span<const uint8_t>> spans);
    void maybe_start();
    void add_remote_info(const EndpointInfo& info);

private:
    void do_start();
    void post_task(const std::function<void()>& task);
    void post_delayed_task(uint32_t delayed_ms, const std::function<void()>& task);
    void create_lan_endpoint();
    void create_wan_endpoint();
    void create_relay_endpoint_after_3s();
    void create_relay_endpoint();

    void on_endpoint_info(const EndpointInfo& info);
    void on_read(Endpoint* ep, const uint8_t* data, uint32_t size, int64_t time_us);
    void on_connected(Endpoint* ep);

private:
    const bool is_server_;
    Endpoint* connected_ep_ = nullptr;
    NetworkChannel* network_channel_;
    const std::string password_;
    const std::string username_;
    Address stun_;
    Address relay_addr_;
    const std::string relay_username_;
    const std::string relay_password_;
    std::function<void(const EndpointInfo&)> on_endpoint_info_gathered_;
    std::function<void(const uint8_t*, uint32_t, int64_t)> on_read_;
    std::function<void(const EndpointInfo& local, const EndpointInfo& remote, int64_t used_time_ms)>
        on_conn_changed_;
    std::function<void(int32_t)> on_error_;
    bool already_started_ = false;
    std::shared_ptr<LanEndpoint> lan_;
    std::shared_ptr<WanEndpoint> wan_;
    std::shared_ptr<RelayEndpoint> relay_;
};

} // namespace rtc2