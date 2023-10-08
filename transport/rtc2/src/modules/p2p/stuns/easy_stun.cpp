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

#include <cstdint>
#include <memory>

#include <modules/p2p/stuns/easy_stun.h>
#include <modules/p2p/stuns/message.h>

namespace rtc2 {

StunMessage::StunMessage(Type type, const uint8_t id[12]) {
    switch (type) {
    case Type::BindingRequest:
        msg_ = std::make_shared<stun::message>(stun::message::binding_request, id);
        break;
    case Type::ChangePortRequest:
        msg_ = std::make_shared<stun::message>(stun::message::binding_request, id);
        *msg_ << stun::attribute::change_request(0x2);
        break;
    case Type::BindingResponse:
        msg_ = std::make_shared<stun::message>(stun::message::binding_response, id);
        break;
    case Type::BindingErrorResponse:
    case Type::Unknown:
    default:
        msg_ = std::make_shared<stun::message>();
        break;
    }
}

StunMessage::StunMessage(const uint8_t* begin, const uint8_t* end)
    : msg_{std::make_shared<stun::message>(begin, end)} {}

bool StunMessage::verify() const {
    return msg_->verify();
}

StunMessage::Type StunMessage::type() const {
    // 无法分辨BindingRequest, ChangePortRequest,
    switch (msg_->type()) {
    case stun::message::binding_request:
        return Type::BindingRequest;
    case stun::message::binding_response:
        return Type::BindingResponse;
    case stun::message::binding_error_response:
        return Type::BindingErrorResponse;
    default:
        return Type::Unknown;
    }
}

const uint8_t* StunMessage::data() const {
    return msg_->data();
}

std::vector<uint8_t> StunMessage::id() const {
    return msg_->id();
}

uint8_t* StunMessage::data() {
    return msg_->data();
}

size_t StunMessage::size() const {
    return msg_->size();
}

std::optional<Address> StunMessage::mapped_address() const {
    std::optional<sockaddr_in> saddr;
    for (auto& attr : *msg_) {
        sockaddr_in addr{};
        if (attr.type() == stun::attribute::type::xor_mapped_address) {
            attr.to<stun::attribute::type::xor_mapped_address>().to_sockaddr((sockaddr*)&addr);
            saddr = addr;
            break;
        }
        else if (attr.type() == stun::attribute::type::mapped_address) {
            attr.to<stun::attribute::type::mapped_address>().to_sockaddr((sockaddr*)&addr);
            saddr = addr;
            break;
        }
    }
    if (!saddr.has_value()) {
        return std::nullopt;
    }
    Address address{IPv4{saddr->sin_addr}, ntohs(saddr->sin_port)};
    return address;
}

} // namespace rtc2