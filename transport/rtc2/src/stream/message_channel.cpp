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
#include "message_channel.h"

#include <span>

#include <modules/message/half_reliable_message_channel.h>
#include <modules/message/reliable_message_channel.h>

namespace rtc2 {

std::shared_ptr<MessageChannel> MessageChannel::create(const Params& params) {
    std::shared_ptr<MessageChannel> channel{new MessageChannel{params}};
    // 构造函数不能用weak_from_this
    params.network_channel->postDelay(
        10 /*ms*/,
        std::bind(&MessageChannel::periodicUpdate, channel.get(), channel->weak_from_this()));
    return channel;
}

MessageChannel::MessageChannel(const Params& params)
    : reliable_ssrc_{params.reliable_ssrc}
    , half_reliable_ssrc_{params.half_reliable_ssrc}
    , dtls_{params.dtls}
    , network_channel_{params.network_channel}
    , callback_thread_{params.callback_thread}
    , on_message_{params.on_message} {
    ReliableMessageChannel::Params reliable_params{};
    reliable_params.ssrc = reliable_ssrc_;
    reliable_params.send_to_network = std::bind(&MessageChannel::sendToNetwork, this,
                                                std::placeholders::_1, std::placeholders::_2);
    reliable_params.on_recv = std::bind(&MessageChannel::onRecvReliable, this,
                                        std::placeholders::_1, std::placeholders::_2);
    reliable_ = std::make_shared<ReliableMessageChannel>(reliable_params);
}

// 跑在用户线程
bool MessageChannel::sendMessage(const uint8_t* data, uint32_t size, bool reliable) {
    (void)reliable;
    // 除了Thread::Invoke写法，似乎避免不了复制两次的命运
    // TODO: 封装成消息，而不是流
    std::vector<uint8_t> message(data, data + size);
    network_channel_->post([this, msg = std::move(message)]() {
        reliable_->sendData(msg.data(), static_cast<uint32_t>(msg.size()));
    });
    return true;
}

// 跑在网络线程
void MessageChannel::onRecvData(const uint8_t* data, uint32_t size, int64_t time_us) {
    (void)time_us;
    reliable_->recvFromNetwork(data, size);
}

void MessageChannel::sendToNetwork(const uint8_t* data, uint32_t size) {
    dtls_->sendPacket(data, size, false);
}

void MessageChannel::onRecvReliable(const uint8_t* data, uint32_t size) {
    // TODO: 解析成消息，而不是流
    std::vector<uint8_t> buffer(data, data + size);
    callback_thread_->post([this, buff = std::move(buffer)]() {
        on_message_(buff.data(), static_cast<uint32_t>(buff.size()), true /*is_reliable*/);
    });
}

void MessageChannel::periodicUpdate(std::weak_ptr<MessageChannel> weak_this) {
    auto shared_this = weak_this.lock();
    if (shared_this) {
        reliable_->periodicUpdate();
        // 因为当前线程模型不支持取消task，只能固定10ms调一次
        network_channel_->postDelay(
            10 /*ms*/, std::bind(&MessageChannel::periodicUpdate, this, weak_from_this()));
    }
}

} // namespace rtc2