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
#include <modules/p2p/endpoint.h>

namespace rtc2 {

class LanEndpoint : public Endpoint {
public:
    struct Params {
        std::vector<Address> addrs;
        std::function<void(const EndpointInfo&)> on_endpoint_info;
        std::function<void(Endpoint*)> on_connected;
        std::function<void(Endpoint*, const uint8_t*, uint32_t, int64_t)> on_read;
        NetworkChannel* network_channel;
    };

public:
    static std::unique_ptr<LanEndpoint> create(const Params& params);
    int32_t send(std::vector<std::span<const uint8_t>> spans) override;
    EndpointType type() const override;

private:
    LanEndpoint(std::unique_ptr<UDPSocket>&& socket, NetworkChannel* network_channel,
                std::function<void(Endpoint*)> on_connected,
                std::function<void(Endpoint*, const uint8_t*, uint32_t, int64_t)> on_read);
    void on_binding_request(const StunMessage& msg, const Address& remote_addr,
                            const int64_t& packet_time_us) override;
    void on_binding_response(const StunMessage& msg, const Address& remote_addr,
                             const int64_t& packet_time_us) override;
    void add_remote_info(const EndpointInfo& info) override;
    void send_binding_requests();

private:
    struct AddressInfo {
        Address addr;
        bool received_request = false;
        bool received_response = false;
    };
    int index_ = -1;
    std::vector<AddressInfo> addr_infos_;
};

} // namespace rtc2