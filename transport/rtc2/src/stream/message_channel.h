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
#include <functional>
#include <memory>

#include <ltlib/threads.h>

#include <modules/dtls/dtls_channel.h>
#include <modules/network/network_channel.h>

namespace rtc2 {

class ReliableMessageChannel;
class HalfReliableMessageChannel;

// 基于message而不是stream的传输通道
// 接口区分reliable和half reliable（即最多重传n次）
// 直接用sctp就不需要分reliable和half reliable，但这样将来就不好在sctp上做优化
//
// 当前拍脑袋的选择是reliable用kcp。
// reliable主要传输一些控制数据，暂时不考虑文件传输。
// 控制数据的特点是“这个时间窗口内产生的数据就是这么多，你必须传输过去”，所以“跑满带宽”并不是reliable的指标
// 它的任务就是尽量降低延迟，只是不清楚它“占用额外的带宽”会不会影响到音视频流，待实际考察
//
// half reliable暂时不实现，统统走reliable，等做完系统的其它部分再回来实现，这个不难
class MessageChannel : public std::enable_shared_from_this<MessageChannel> {
public:
    struct Params {
        uint32_t reliable_ssrc;
        uint32_t half_reliable_ssrc;
        DtlsChannel* dtls;
        NetworkChannel* network_channel;
        ltlib::TaskThread* callback_thread;
        std::function<void(const uint8_t* data, uint32_t size, bool reliable)> on_message;
        int mtu;
        int sndwnd;
        int rcvwnd;
    };

public:
    MessageChannel(const Params& params);
    bool sendMessage(const uint8_t* data, uint32_t size, bool reliable);
    void onRecvData(const uint8_t* data, uint32_t size, int64_t time_us);

private:
    void sendToNetwork(const uint8_t* data, uint32_t size);
    void onRecvReliable(const uint8_t* data, uint32_t size);
    void periodicUpdate(std::weak_ptr<MessageChannel> weak_this);

private:
    uint32_t reliable_ssrc_;
    uint32_t half_reliable_ssrc_;
    DtlsChannel* dtls_;
    NetworkChannel* network_channel_;
    ltlib::TaskThread* callback_thread_;
    std::function<void(const uint8_t* data, uint32_t size, bool reliable)> on_message_;
    std::shared_ptr<ReliableMessageChannel> reliable_;
    std::shared_ptr<HalfReliableMessageChannel> half_reliable_;
};

} // namespace rtc2