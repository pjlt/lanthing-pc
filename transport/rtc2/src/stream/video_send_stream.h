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
#include <memory>

#include <rtc2/connection.h>
#include <rtc2/video_frame.h>

#include <modules/cc/pacer.h>
#include <modules/network/network_channel.h>
#include <modules/rtp/rtp_packet.h>

namespace rtc2 {
class VideoSendStream {
public:
    struct Params {
        uint32_t ssrc;
        Pacer* pacer;
        std::function<void()> on_request_keyframe;
    };

public:
    VideoSendStream(const Params& params);
    void sendFrame(const VideoFrame& frame);
    uint32_t ssrc() const;
    void onRtcpPacket(const uint8_t* data, uint32_t size, int64_t time_us);

private:
    std::vector<PacedPacket> packetize(const VideoFrame& frame);
    void onPcedPacket(RtpPacket& packet);
    void protectAndSendPacket(const RtpPacket& packet);

private:
    uint32_t ssrc_;
    std::function<void()> on_request_keyframe_;
    NetworkChannel* network_channel_;
    Pacer* pacer_;
    uint16_t rtp_seq_;
    uint16_t rtx_seq_;
};
} // namespace rtc2