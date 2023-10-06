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

#include "reliable_message_channel.h"
#include "ikcp.h"

#include <g3log/g3log.hpp>

#include <ltlib/times.h>

namespace rtc2 {

ReliableMessageChannel::ReliableMessageChannel(const Params& params)
    : ssrc_{params.ssrc}
    , send_to_network_{params.send_to_network}
    , on_recv_{params.on_recv}
    , kcp_{ikcp_create(params.ssrc, this)} {
    // 运行过程中可不可以修改？待确认
    ikcp_setmtu(kcp_, params.mtu);
    ikcp_setoutput(kcp_, &ReliableMessageChannel::onKcpOutput);
    ikcp_wndsize(kcp_, params.sndwnd, params.rcvwnd);
    ikcp_nodelay(kcp_, 1, 10, 2, 1); // 这样子设置，最小RTO就是30ms，这个值是否合理？
    buffer_.resize(1024 * 1024);
}

ReliableMessageChannel::~ReliableMessageChannel() {
    ikcp_release(kcp_);
}

bool ReliableMessageChannel::sendData(const uint8_t* data, uint32_t size) {
    int ret = ikcp_send(kcp_, reinterpret_cast<const char*>(data), static_cast<int>(size));
    return ret >= 0;
}

bool ReliableMessageChannel::recvFromNetwork(const uint8_t* data, uint32_t size) {
    int ret = ikcp_input(kcp_, reinterpret_cast<const char*>(data), static_cast<int>(size));
    if (ret < 0) {
        LOG(WARNING) << "ikcp_input " << ret;
        return false;
    }
    ret = ikcp_recv(kcp_, buffer_.data(), static_cast<int>(buffer_.size()));
    if (ret >= 0) {
        on_recv_(reinterpret_cast<const uint8_t*>(buffer_.data()), ret);
    }
    return true;
}

void ReliableMessageChannel::periodicUpdate() {
    ikcp_update(kcp_, static_cast<uint32_t>(ltlib::steady_now_ms()));
}

int ReliableMessageChannel::onKcpOutput(const char* buf, int len, ikcpcb* kcp, void* user) {
    (void)kcp;
    auto that = reinterpret_cast<ReliableMessageChannel*>(user);
    that->send_to_network_(reinterpret_cast<const uint8_t*>(buf), static_cast<uint32_t>(len));
    return len;
}

} // namespace rtc2