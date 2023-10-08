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

#include "endpoint.h"

#include <ltlib/logging.h>
#include <ltlib/strings.h>

namespace rtc2 {

void Endpoint::post_task(const std::function<void()>& task) {
    auto weak_this = weak_from_this();
    network_channel_->post([weak_this, task]() {
        auto shared_this = weak_this.lock();
        if (shared_this == nullptr) {
            return;
        }
        task();
    });
}

void Endpoint::post_delayed_task(uint32_t delayed_ms, const std::function<void()>& task) {
    auto weak_this = weak_from_this();
    network_channel_->postDelay(delayed_ms, [weak_this, task]() {
        auto shared_this = weak_this.lock();
        if (shared_this == nullptr) {
            return;
        }
        task();
    });
}

Endpoint::Endpoint(std::unique_ptr<UDPSocket>&& socket, NetworkChannel* network_channel,
                   std::function<void(Endpoint*)> on_connected,
                   std::function<void(Endpoint*, const uint8_t*, uint32_t, int64_t)> on_read)
    : socket_{std::move(socket)}
    , network_channel_{network_channel}
    , on_connected_{on_connected}
    , on_read_{on_read} {}

UDPSocket* rtc2::Endpoint::sock() {
    return socket_.get();
}

void Endpoint::send_binding_request(const Address& addr) {
    if (connected()) {
        return;
    }
    std::string id = ltlib::randomStr(kStunTsxIDLen);
    StunMessage msg{StunMessage::Type::BindingRequest, reinterpret_cast<const uint8_t*>(id.data())};
    int ret = sock()->sendmsg({{msg.data(), msg.size()}}, addr);
    if (ret < 0) {
        int error = socket_->error();
        LOG(ERR) << "Send binding request to " << addr.to_string() << " failed with error "
                 << error;
    }
}

bool Endpoint::connected() const {
    return received_request_ && received_response_;
}

void Endpoint::set_received_request() {
    received_request_ = true;
    maybe_connected();
}

void Endpoint::set_received_response() {
    received_response_ = true;
    maybe_connected();
}

void Endpoint::maybe_connected() {
    if (connected()) {
        on_connected_(this);
    }
}

void Endpoint::on_read(std::weak_ptr<Endpoint> weak_this, const uint8_t* data, uint32_t size,
                       const Address& remote_addr, const int64_t& packet_time_us) {
    auto shared_this = weak_this.lock();
    if (shared_this == nullptr) {
        return;
    }
    StunMessage msg{reinterpret_cast<const uint8_t*>(data),
                    reinterpret_cast<const uint8_t*>(data) + size};
    if (msg.verify()) {
        switch (msg.type()) {
        case StunMessage::Type::BindingRequest:
            on_binding_request(msg, remote_addr, packet_time_us);
            break;
        case StunMessage::Type::BindingResponse:
            on_binding_response(msg, remote_addr, packet_time_us);
            break;
        default:
            LOG(WARNING) << "Unsupported StunMessageType " << (int)msg.type();
            break;
        }
    }
    else {
        if (connected()) {
            on_read_(this, data, size, packet_time_us);
        }
    }
}

void Endpoint::init() {
    // 构造函数不能用weak_from_this()/shared_from_this()，只能另搞一个init函数
    socket_->setOnRead(std::bind(&Endpoint::on_read, this, weak_from_this(), std::placeholders::_1,
                                 std::placeholders::_2, std::placeholders::_3,
                                 std::placeholders::_4));
}

} // namespace rtc2
