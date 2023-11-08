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

#include <concepts>
#include <span>

#include <modules/buffer.h>

namespace rtc2 {

enum class RTPExtensionType : int {
    kRtpExtensionNone,
    kRtpExtensionLtPacketInfo,
    kRtpExtensionLtFrameInfo,
    kRtpExtensionNumberOfExtensions // Must be the last entity in the enum.
};

template <typename T>
concept RtpExtension = requires(Buffer buff, typename T::value_type& ref_value,
                                const typename T::value_type& const_value) {
    { T::id() } -> std::same_as<RTPExtensionType>;
    { T::value_size(const_value) } -> std::same_as<uint8_t>;
    { T::uri() } -> std::same_as<const char*>;
    { T::read_from_buff(buff, ref_value) } -> std::same_as<bool>;
    { T::write_to_buff(buff, const_value) } -> std::same_as<bool>;
};

class LtPacketInfo {
public:
    bool is_first_packet_in_frame() const { return first_packet_; }

    void set_first_packet_in_frame(bool value) { first_packet_ = value; };

    bool is_last_packet_in_frame() const { return last_packet_; }

    void set_last_packet_in_frame(bool value) { last_packet_ = value; };

    bool is_keyframe() const { return key_frame_; }

    void set_keyframe(bool value) { key_frame_ = value; };

    bool is_retransmit() const { return retransmit_; }

    void set_retransmit(bool value) { retransmit_ = value; };

    uint16_t sequence_number() const { return seq_; }

    void set_sequence_number(uint16_t seq) { seq_ = seq; }

private:
    bool first_packet_ = false;
    bool last_packet_ = false;
    bool key_frame_ = false;
    bool retransmit_ = false;
    uint16_t seq_ = 0;
};

class LtPacketInfoExtension {
public:
    using value_type = LtPacketInfo;

    static RTPExtensionType id() { return RTPExtensionType::kRtpExtensionLtPacketInfo; }

    static const char* uri() { return "lanthing-packet-info"; }

    static uint8_t value_size(const LtPacketInfo&) { return 3; }

    static bool read_from_buff(Buffer buff, LtPacketInfo& info);

    static bool write_to_buff(Buffer buff, const LtPacketInfo& info);
};

class LtFrameInfo {
public:
    uint16_t frame_id() const { return frame_id_; }

    void set_frame_id(uint16_t id) { frame_id_ = id; }

    uint16_t encode_duration() const { return encode_duration_; }

    void set_encode_duration(uint16_t duration) { encode_duration_ = duration; }

private:
    uint16_t frame_id_ = 0;
    uint16_t encode_duration_ = 0;
};

class LtFrameInfoExtension {
public:
    using value_type = LtFrameInfo;

    static RTPExtensionType id() { return RTPExtensionType::kRtpExtensionLtFrameInfo; }

    static const char* uri() { return "lanthing-frame-info"; }

    static uint8_t value_size(const LtFrameInfo&) { return 4; }

    static bool read_from_buff(Buffer buff, LtFrameInfo& info);

    static bool write_to_buff(Buffer buff, const LtFrameInfo& info);
};

} // namespace rtc2