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

#include "rtp_extention.h"

#include <cassert>

#include <algorithm>

namespace {

constexpr uint8_t kFirstPacketInFrame = 0b0000'0001;
constexpr uint8_t kLastPacketInFrame = 0b0000'0010;
constexpr uint8_t kKeyFrame = 0b0000'0100;
constexpr uint8_t kRetransmit = 0b0000'1000;

} // namespace

namespace rtc2 {

bool LtPacketInfoExtension::read_from_buff(Buffer buff, LtPacketInfo& info) {
    if (buff.size() < value_size(info)) {
        return false;
    }
    auto span = buff.spans()[0];
    if (span.empty()) {
        return false;
    }
    info.set_first_packet_in_frame((span[0] & kFirstPacketInFrame) != 0);
    info.set_last_packet_in_frame((span[0] & kLastPacketInFrame) != 0);
    info.set_keyframe((span[0] & kKeyFrame) != 0);
    info.set_retransmit((span[0] & kRetransmit) != 0);
    info.set_sequence_number(*(uint16_t*)(span.data() + 1));
    return true;
}

bool LtPacketInfoExtension::write_to_buff(Buffer buff, const LtPacketInfo& info) {
    if (buff.size() < value_size(info)) {
        return false;
    }
    auto span = buff.spans()[0];
    if (span.size() != value_size(info)) {
        std::abort();
    }
    span[0] |= info.is_first_packet_in_frame() ? kFirstPacketInFrame : 0;
    span[0] |= info.is_last_packet_in_frame() ? kLastPacketInFrame : 0;
    span[0] |= info.is_keyframe() ? kKeyFrame : 0;
    span[0] |= info.is_retransmit() ? kRetransmit : 0;
    *(uint16_t*)(span.data() + 1) = info.sequence_number();
    return true;
}

bool LtFrameInfoExtension::read_from_buff(Buffer buff, LtFrameInfo& info) {
    if (buff.size() < value_size(info)) {
        return false;
    }
    auto span = buff.spans()[0];
    if (span.empty()) {
        return false;
    }
    info.set_frame_id(*(uint16_t*)(span.data() + 0));
    info.set_encode_duration(*(uint16_t*)(span.data() + 2));
    return true;
}

bool LtFrameInfoExtension::write_to_buff(Buffer buff, const LtFrameInfo& info) {
    if (buff.size() < value_size(info)) {
        return false;
    }
    auto span = buff.spans()[0];
    if (span.size() != value_size(info)) {
        std::abort();
    }
    *(uint16_t*)(span.data() + 0) = info.frame_id();
    *(uint16_t*)(span.data() + 2) = info.encode_duration();
    return true;
}

} // namespace rtc2