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

#include <rtc2/connection.h>

#include "connection_impl.h"

namespace rtc2 {

std::shared_ptr<Connection> Connection::create(const Params& params) {
    auto impl = std::make_shared<ConnectionImpl>(params);
    if (impl->init()) {
        std::shared_ptr<Connection> conn{new Connection};
        conn->impl_ = impl;
        return conn;
    }
    else {
        return nullptr;
    }
}

void Connection::start() {
    impl_->start();
}

bool Connection::sendData(uint32_t ssrc, const uint8_t* data, uint32_t size) {
    (void)ssrc;
    return impl_->sendData(data, size);
}

bool Connection::sendVideo(uint32_t ssrc, const VideoFrame& frame) {
    return impl_->sendVideo(ssrc, frame);
}

bool Connection::sendAudio(uint32_t ssrc, const uint8_t* data, uint32_t size) {
    return impl_->sendAudio(ssrc, data, size);
}

void Connection::onSignalingMessage(const std::string& key, const std::string& value) {
    impl_->onSignalingMessage(key, value);
}

} // namespace rtc2