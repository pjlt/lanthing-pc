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

#include <cassert>

#include <rtc2/video_frame.h>

#include "video_send_stream.h"

namespace rtc2 {

VideoSendStream::VideoSendStream(const Params& params)
    : ssrc_{params.ssrc}
    , on_request_keyframe_{params.on_request_keyframe}
    , pacer_{params.pacer} {
    constexpr uint16_t kMaxInitRtpSeqNumber = 0x7fff; // 2^15 - 1.
    rtp_seq_ = static_cast<uint16_t>(std::min(1, rand() % kMaxInitRtpSeqNumber));
    rtx_seq_ = static_cast<uint16_t>(std::min(1, rand() % kMaxInitRtpSeqNumber));
}

// 跑在用户线程
void VideoSendStream::sendFrame(const VideoFrame& frame) {
    // 用到两个扩展，分别是在每个packet添加的LtPacketInfoExtension，和在首包添加的LtFrameInfoExtension
    // 注意，LtPacketInfoExtension还有一个字段sequence_number没有在此处赋值
    auto packets = packetize(frame);
    for (size_t i = 0; i < packets.size(); i++) {
        LtPacketInfo pkinfo{};
        pkinfo.set_keyframe(frame.is_keyframe);
        pkinfo.set_retransmit(false);
        if (i == 0) {
            pkinfo.set_first_packet_in_frame(true);
            LtFrameInfo finfo{};
            finfo.set_frame_id(frame.frame_id & 0xFFFF);
            // 最小时间单位150us，uint16能表示最大时间为 65535 * 150us = 9830250us = 9.83s
            finfo.set_encode_duration(static_cast<uint16_t>(frame.encode_duration_us / 150));
            packets[i].rtp.set_extension<LtFrameInfoExtension>(finfo);
        }
        if (i == packets.size() - 1) {
            pkinfo.set_last_packet_in_frame(true);
        }
        packets[i].rtp.set_extension<LtPacketInfoExtension>(pkinfo);
    }
    pacer_->enqueuePackets(std::move(packets));
}

uint32_t VideoSendStream::ssrc() const {
    return ssrc_;
}

void VideoSendStream::onRtcpPacket(const uint8_t* data, uint32_t size, int64_t time_us) {
    (void)data;
    (void)size;
    (void)time_us;
}

std::vector<PacedPacket> VideoSendStream::packetize(const VideoFrame& frame) {
    // TODO: 1. 探测MTU 2.获取底层连接是IPv4还是IPv6
    constexpr uint32_t kMTU = 1450;
    constexpr uint32_t kIPv6HeaderSize = 40;
    constexpr uint32_t kUDPHeaderSize = 8;
    constexpr uint32_t kRtpHeaderSize = 12;
    constexpr uint32_t kMaxPacketSize = kMTU - kIPv6HeaderSize - kUDPHeaderSize - kRtpHeaderSize;
    uint32_t kRtpPkInfoExtSize = LtPacketInfoExtension::value_size(LtPacketInfo{});
    uint32_t kRtpFrameInfoExtSize = LtFrameInfoExtension::value_size(LtFrameInfo{});

    std::vector<PacedPacket> packets;
    uint32_t offset = 0;
    bool first_packet = true;
    while (offset < frame.size) {
        PacedPacket pk;
        LtPacketInfo packet_info{};
        uint32_t packet_size = kMaxPacketSize - kRtpPkInfoExtSize;
        if (first_packet) {
            packet_size -= kRtpFrameInfoExtSize;
            LtFrameInfo frame_info{};
            frame_info.set_encode_duration(static_cast<uint16_t>(frame.encode_duration_us / 150));
            frame_info.set_frame_id(static_cast<uint16_t>(frame.frame_id & 0xFFFF));
            pk.rtp.set_extension<LtFrameInfoExtension>(frame_info);
            packet_info.set_first_packet_in_frame(true);
            first_packet = false;
        }
        if (offset + packet_size >= frame.size) {
            packet_size = frame.size - offset;
            packet_info.set_last_packet_in_frame(true);
        }
        packet_info.set_retransmit(false);
        packet_info.set_keyframe(frame.is_keyframe);
        pk.rtp.set_extension<LtPacketInfoExtension>(packet_info);
        std::span<const uint8_t> span(frame.data + offset, packet_size);
        // 必须设置完所有extension后才能设置payload
        pk.rtp.set_ssrc(ssrc_);
        pk.rtp.set_timestamp(
            static_cast<uint32_t>(frame.encode_timestamp_us / 1000)); // 没有必要搞一层采样率
        pk.rtp.set_payload_type(125);
        pk.rtp.set_payload(span);
        pk.send_func = std::bind(&VideoSendStream::onPcedPacket, this, std::placeholders ::_1);
        packets.push_back(std::move(pk));
        offset += packet_size;
    }
    return packets;
}

// 跑在pacer/cc线程
void VideoSendStream::onPcedPacket(RtpPacket& packet) {
    // TODO: cc
    packet.set_sequence_number(rtp_seq_++); // TODO: retransmit用独立的seq
    network_channel_->post(
        std::bind(&VideoSendStream::protectAndSendPacket, this, std::move(packet)));
}

// 跑在网络线程
void VideoSendStream::protectAndSendPacket(const RtpPacket& packet) {
    // TODO: protect...
    network_channel_->sendPacket(packet.buff().spans());
}

} // namespace rtc2