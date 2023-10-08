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
#include <optional>
#include <vector>

#include <modules/network/address.h>

namespace stun {

template <typename T> class base_message;

} // namespace stun

namespace rtc2 {

class StunMessage {
public:
    enum class Type : uint16_t {
        Unknown,
        BindingRequest,
        ChangePortRequest,
        BindingResponse,
        BindingErrorResponse,
    };

public:
    StunMessage(Type type, const uint8_t id[12]);
    StunMessage(const uint8_t* begin, const uint8_t* end);
    bool verify() const;
    Type type() const;
    const uint8_t* data() const;
    std::vector<uint8_t> id() const;
    uint8_t* data();
    size_t size() const;
    std::optional<Address> mapped_address() const;

private:
    std::shared_ptr<stun::base_message<std::allocator<uint8_t>>> msg_;
};

} // namespace rtc2