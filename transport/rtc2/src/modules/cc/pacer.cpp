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

#include "pacer.h"

#include <cassert>

namespace rtc2 {

Pacer::Pacer(const Params& params)
    : post_task_{params.post_task}
    , post_delayed_task_{params.post_delayed_task} {}

void Pacer::enqueuePackets(std::vector<PacedPacket>&& packets) {
    std::lock_guard lock{mutex_};
    for (size_t i = 0; i < packets.size(); i++) {
        queue_.push_back(std::move(packets[i]));
    }
}

void Pacer::process(std::weak_ptr<Pacer> weak_this) {
    auto that = weak_this.lock();
    if (that == nullptr) {
        return;
    }
    std::deque<PacedPacket> packets;
    {
        std::lock_guard lock{mutex_};
        packets = std::move(queue_);
    }
    for (size_t i = 0; i < packets.size(); i++) {
        LtPacketInfo pkinfo{};
        bool ok = packets[i].rtp.get_extension<LtPacketInfoExtension>(pkinfo);
        (void)ok;
        assert(ok);
        pkinfo.set_sequence_number(static_cast<uint16_t>(++global_seq_) & 0xFFFF);
        packets[i].rtp.set_extension<LtPacketInfoExtension>(pkinfo);
        packets[i].send_func(packets[i].rtp);
    }
    post_delayed_task_(1 /*ms*/, std::bind(&Pacer::process, this, weak_from_this()));
}

} // namespace rtc2