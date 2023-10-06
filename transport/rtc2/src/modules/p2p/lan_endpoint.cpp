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

#include "lan_endpoint.h"

namespace rtc2 {

std::unique_ptr<LanEndpoint> LanEndpoint::create(const Params& params) {
    auto udp_socket = params.network_channel->createUDPSocket(params.addr);
    if (udp_socket == nullptr) {
        return nullptr;
    }

    std::unique_ptr<LanEndpoint> ep{new LanEndpoint(std::move(udp_socket), params.network_channel,
                                                    params.on_connected, params.on_read)};
    ep->init();
    EndpointInfo info{};
    info.address = params.addr;
    info.type = EndpointType::Lan;
    ep->set_local_info(info);
    params.on_endpoint_info(info);
    return ep;
}

int32_t LanEndpoint::send(std::vector<std::span<const uint8_t>> spans) {
    return sock()->sendmsg(spans, remote_info().address);
}

EndpointType LanEndpoint::type() const {
    return EndpointType::Lan;
}

LanEndpoint::LanEndpoint(std::unique_ptr<UDPSocket>&& socket, NetworkChannel* network_channel,
                         std::function<void(Endpoint*)> on_connected,
                         std::function<void(Endpoint*, const uint8_t*, uint32_t, int64_t)> on_read)
    : Endpoint{std::move(socket), network_channel, on_connected, on_read} {}

void LanEndpoint::on_binding_request(const StunMessage& msg, const Address& remote_addr,
                                     const int64_t& packet_time_us) {
    (void)msg;
    (void)packet_time_us;
    // 此处只验证对方IP:Port，不浪费更多资源，因为上面还有一层DTLS验证
    // 后面如果确实想要在此处做进一步验证，可以用传入的p2p_username和p2p_password做HMAC校验
    if (remote_addr != remote_info().address) {
        return;
    }
    set_received_request();
}

void LanEndpoint::on_binding_response(const StunMessage& msg, const Address& remote_addr,
                                      const int64_t& packet_time_us) {
    (void)msg;
    (void)packet_time_us;
    // 此处只验证对方IP:Port，不浪费更多资源，因为上面还有一层DTLS验证
    // 后面如果确实想要在此处做进一步验证，可以用传入的p2p_username和p2p_password做HMAC校验
    if (remote_addr != remote_info().address) {
        return;
    }
    set_received_response();
}
} // namespace rtc2