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
#include "video_receive_stream.h"

#include <g3log/g3log.hpp>

#include <rtc2/video_frame.h>

namespace {
constexpr size_t kStartPacketBufferSize = 512;
constexpr size_t kMaxPacketBufferSize = 1000;
constexpr size_t kDecodedHistorySize = 1000;
} // namespace

namespace rtc2 {

VideoReceiveStream::VideoReceiveStream(const Params& param)
    : ssrc_{param.ssrc}
    , on_decodable_frame_{param.on_decodable_frame}
    , frame_assembler_(kStartPacketBufferSize, kMaxPacketBufferSize) {}

uint32_t VideoReceiveStream::ssrc() const {
    return ssrc_;
}

void VideoReceiveStream::onRtcpPacket(const uint8_t* data, uint32_t size, int64_t time_us) {
    (void)data;
    (void)size;
    (void)time_us;
}

// 跑在网络线程
void VideoReceiveStream::onRtpPacket(const uint8_t* data, uint32_t size, int64_t time_us) {
    // unprotect
    std::span<const uint8_t> sp(data, size);
    std::optional<RtpPacket> packet = RtpPacket::fromBuffer(Buffer{sp});
    if (!packet.has_value()) {
        LOG(WARNING) << "Parse rtp packet failed";
        return;
    }
    thread_->post(
        std::bind(&VideoReceiveStream::onUnprotectedRtpPacket, this, packet.value(), time_us));
}

void VideoReceiveStream::onUnprotectedRtpPacket(const RtpPacket& packet, int64_t time_us) {
    (void)time_us;
    VideoPacket video_packet{packet};
    auto result = frame_assembler_.insert(video_packet);
    if (result.buffer_cleared) {
        // TODO:请求I帧
        return;
    }
    if (!result.packets.empty()) {
        uint32_t size = 0;
        for (auto& pkt : result.packets) {
            size += static_cast<uint32_t>(pkt.rtp.size());
        }
        std::vector<uint8_t> data;
        data.resize(size);
        uint32_t offset = 0;
        VideoFrame video_frame{};

        for (auto& pkt : result.packets) {
            if (pkt.frame_id.has_value()) {
                video_frame.frame_id = frame_id_unwrapper_.Unwrap(pkt.frame_id.value());
                video_frame.encode_duration_us =
                    static_cast<uint64_t>(pkt.encode_duration.value()) * 150;
            }
            // 理论上spans.size() == 1
            auto spans = pkt.rtp.buff().spans();
            for (auto span : spans) {
                memcpy(data.data() + offset, span.data(), span.size());
                offset += static_cast<uint32_t>(span.size());
            }
        }
        video_frame.data = data.data();
        video_frame.size = size;
        on_decodable_frame_(video_frame);
    }
}

} // namespace rtc2