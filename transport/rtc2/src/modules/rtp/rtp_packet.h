/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#pragma once
#include <cassert>

#include <optional>
#include <span>
#include <vector>

#include <modules/buffer.h>
#include <modules/rtp/rtp_extention.h>

namespace rtc2 {

constexpr uint32_t kOneByteHeaderExtensionMaxId = 14;
constexpr uint32_t kOneByteHeaderExtensionMaxValueSize = 16;

class RtpPacket {
public:
    RtpPacket();
    static std::optional<RtpPacket> fromBuffer(Buffer buff);

    bool marker() const;
    uint8_t payload_type() const;
    uint16_t sequence_number() const;
    uint32_t timestamp() const;
    uint32_t ssrc() const;
    std::vector<uint32_t> csrcs() const;
    size_t csrcs_size() const;
    size_t headers_size() const;
    size_t payload_size() const;
    size_t padding_size() const;
    size_t extensions_size() const;
    template <typename T>
        requires RtpExtension<T>
    bool get_extension(typename T::value_type& ext) const;
    const Buffer payload() const;
    size_t size() const;
    bool empty_payload() const;
    const Buffer buff() const;
    // template <typename T> const T& video_header() const { return std::get<T>(video_header_); }
    // template <typename T> T& video_header() { return std::get<T>(video_header_); }
    //  const ExtraRtpInfo& extra_info() const;
    //  ExtraRtpInfo& extra_info();

    void set_marker(bool marker);
    void set_payload_type(uint8_t pt);
    void set_sequence_number(uint16_t seq);
    void set_timestamp(uint32_t timestamp);
    void set_ssrc(uint32_t ssrc);
    void set_csrcs(std::span<uint32_t> csrcs);
    template <typename T>
        requires RtpExtension<T>
    bool set_extension(const typename T::value_type& ext);
    void set_payload(const std::span<const uint8_t>& payload);
    void set_payload(std::vector<uint8_t>&& payload);

private:
    RtpPacket(Buffer buff);
    bool parse();
    Buffer find_extension(RTPExtensionType type) const;

    template <typename T>
        requires RtpExtension<T>
    bool need_promotion(const typename T::value_type& value) const;

    template <typename T>
        requires RtpExtension<T>
    bool need_more_buffer_space() const;

    template <typename T>
        requires RtpExtension<T>
    bool push_back_extension(const typename T::value_type& value);

    void promote_two_bytes_header_and_reserve_n_bytes(uint8_t n_bytes);

    void allocate_n_bytes_for_extension(uint8_t bytes);

private:
    struct ExtensionInfo {
        explicit ExtensionInfo(RTPExtensionType _type)
            : ExtensionInfo(_type, 0, 0) {}
        ExtensionInfo(RTPExtensionType _type, uint16_t _offset, uint8_t _length)
            : type(_type)
            , offset(_offset)
            , length(_length) {}
        RTPExtensionType type;
        uint16_t offset;
        uint8_t length;
    };
    enum class ExtensionMode {
        kOneByte,
        kTwoByte,
    };

    ExtensionInfo& find_or_create_extension_info(RTPExtensionType type);

private:
    ExtensionMode extension_mode_ = ExtensionMode::kOneByte;
    std::vector<ExtensionInfo> extension_entries_;
    // ExtraRtpInfo extra_rtp_info_;
    mutable Buffer buffer_;
    // mutable Frame frame_;
};

template <typename T>
    requires RtpExtension<T>
inline bool RtpPacket::get_extension(typename T::value_type& value) const {
    auto buff = find_extension(T::id());
    if (buff.size() == 0) {
        return false;
    }
    return T::read_from_buff(buff, value);
}

template <typename T>
    requires RtpExtension<T>
inline bool RtpPacket::set_extension(const typename T::value_type& value) {
    buffer_[0] |= 0b0001'0000;
    auto buff = find_extension(T::id());
    if (buff.size() != 0) {
        return T::write_to_buff(buff, value);
    }
    if (need_promotion<T>(value)) {
        promote_two_bytes_header_and_reserve_n_bytes(T::value_size(value) + 2);
    }
    else { // if (need_more_buffer_space<T>()) { 空间不是预留式的，所以不需要need_more_buffer_space
        uint8_t size = extension_mode_ == ExtensionMode::kOneByte ? T::value_size(value) + 1
                                                                  : T::value_size(value) + 2;
        allocate_n_bytes_for_extension(size);
    }
    return push_back_extension<T>(value);
}

template <typename T>
    requires RtpExtension<T>
inline bool RtpPacket::need_promotion(const typename T::value_type& value) const {
    uint32_t id = static_cast<uint32_t>(T::id());
    assert(id != 15 && id != 0);
    return extension_mode_ == ExtensionMode::kOneByte &&
           (id > kOneByteHeaderExtensionMaxId ||
            T::value_size(value) > kOneByteHeaderExtensionMaxValueSize);
}

template <typename T>
    requires RtpExtension<T>
inline bool RtpPacket::need_more_buffer_space() const {
    // extension element之间似乎还有padding，比较麻烦算
    return false;
}

template <typename T>
    requires RtpExtension<T>
bool RtpPacket::push_back_extension(const typename T::value_type& value) {
    constexpr size_t kFixedHeaderSize = 12;
    uint16_t insert_pos;
    if (not extension_entries_.empty()) {
        insert_pos = extension_entries_.back().offset + extension_entries_.back().length;
    }
    else {
        insert_pos = static_cast<uint16_t>(kFixedHeaderSize + csrcs_size() * sizeof(uint32_t) +
                                           sizeof(uint32_t));
    }
    const uint8_t id = static_cast<uint8_t>(T::id());
    const uint8_t value_size = T::value_size(value);
    if (extension_mode_ == ExtensionMode::kOneByte) {
        buffer_[insert_pos] = (id << 4) | (value_size - 1);
        T::write_to_buff(buffer_.subbuf(insert_pos + 1, value_size), value);
        extension_entries_.push_back(ExtensionInfo{T::id(), insert_pos, uint8_t(value_size + 1)});
    }
    else {
        buffer_[insert_pos] = id;
        buffer_[insert_pos + 1] = value_size;
        T::write_to_buff(buffer_.subbuf(insert_pos + 2, value_size), value);
        extension_entries_.push_back(ExtensionInfo{T::id(), insert_pos, uint8_t(value_size + 2)});
    }
    return true;
}

} // namespace rtc2