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
#include <span>
#include <vector>

#include <modules/network/network_channel.h>
#include <modules/p2p/endpoint_info.h>
#include <modules/p2p/stuns/easy_stun.h>

namespace rtc2 {

constexpr uint32_t kStunTsxIDLen = 12;

class Endpoint : public std::enable_shared_from_this<Endpoint> {
public:
    virtual ~Endpoint() = default;
    Endpoint& operator=(const Endpoint&) = delete;
    Endpoint(const Endpoint&) = delete;
    Endpoint& operator=(Endpoint&&) = delete;
    Endpoint(Endpoint&&) = delete;

    void init();

    virtual int32_t send(std::vector<std::span<const uint8_t>> spans) = 0;
    virtual EndpointType type() const = 0;

    virtual void add_remote_info(const EndpointInfo& info) = 0;
    virtual const EndpointInfo& local_info() const = 0;
    virtual const EndpointInfo& remote_info() const = 0;

    void post_task(const std::function<void()>& task);
    void post_delayed_task(uint32_t delayed_ms, const std::function<void()>& task);

protected:
    Endpoint(std::unique_ptr<UDPSocket>&& socket, NetworkChannel* network_channel,
             std::function<void(Endpoint*)> on_connected,
             std::function<void(Endpoint*, const uint8_t*, uint32_t, int64_t)> on_read);
    UDPSocket* sock();
    void send_binding_request(const Address& addr);
    bool connected() const;
    void set_received_request();
    void set_received_response();

private:
    void maybe_connected();
    void on_read(std::weak_ptr<Endpoint> weak_this, const uint8_t* data, uint32_t size,
                 const Address& remote_addr, const int64_t& packet_time_us);
    virtual void on_binding_request(const StunMessage& msg, const Address& remote_addr,
                                    const int64_t& packet_time_us) = 0;
    virtual void on_binding_response(const StunMessage& msg, const Address& remote_addr,
                                     const int64_t& packet_time_us) = 0;

private:
    std::unique_ptr<UDPSocket> socket_;
    std::function<void(Endpoint*, const uint8_t*, uint32_t, int64_t)> on_read_;
    std::function<void(Endpoint*)> on_connected_;
    bool received_request_ = false;
    bool received_response_ = false;
    NetworkChannel* network_channel_;
};

} // namespace rtc2