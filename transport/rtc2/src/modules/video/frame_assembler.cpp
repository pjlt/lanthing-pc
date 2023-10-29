/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "frame_assembler.h"

#include <cassert>

#include <ltlib/logging.h>

#include <ltlib/times.h>

namespace rtc2 {

FrameAssembler::FrameAssembler(size_t start_size, size_t max_size)
    : buffer_(start_size)
    , max_size_(max_size) {}

// RtpExtension传了frameid过来，理论上可以帮助优化，但是没必要，直接用WebRTC的
FrameAssembler::InsertResult FrameAssembler::insert(const VideoPacket& packet) {
    InsertResult result;
    uint16_t seq_num = packet.rtp.sequence_number();
    size_t index = seq_num % buffer_.size();

    if (!first_packet_received_) {
        first_seq_num_ = seq_num;
        first_packet_received_ = true;
    }
    else if (webrtc::AheadOf(first_seq_num_, seq_num)) {
        if (is_cleared_to_first_seq_num_) {
            return result;
        }
        first_seq_num_ = seq_num;
    }

    if (buffer_[index].rtp.size() != 0) {
        if (buffer_[index].rtp.sequence_number() == packet.rtp.sequence_number()) {
            return result;
        }
        while (expand_buffer() && buffer_[seq_num % buffer_.size()].rtp.size() != 0) {
        }
        index = seq_num % buffer_.size();
        if (buffer_[index].rtp.size() != 0) {
            LOG(WARNING) << "Clear PacketBuffer and request key frame.";
            clear_internal();
            result.buffer_cleared = true;
            return result;
        }
    }

    int64_t now_ms = ltlib::steady_now_ms();
    last_received_packet_ms_ = now_ms;
    buffer_[index] = packet;

    update_missing_packets(seq_num);

    result.packets = find_frames(seq_num);
    return result;
}

bool FrameAssembler::expand_buffer() {
    if (buffer_.size() == max_size_) {
        LOG(WARNING) << "PacketBuffer is already at max size (" << max_size_
                     << "), failed to increase size.";
        return false;
    }

    size_t new_size = std::min(max_size_, 2 * buffer_.size());
    std::vector<VideoPacket> new_buffer(new_size);
    for (auto& entry : buffer_) {
        if (entry.rtp.size() != 0) {
            new_buffer[entry.rtp.sequence_number() % new_size] = entry;
        }
    }
    buffer_ = std::move(new_buffer);
    LOG(INFO) << "PacketBuffer size expanded to " << new_size;
    return true;
}

void FrameAssembler::clear_internal() {
    for (auto& entry : buffer_) {
        entry = VideoPacket{};
    }
}

void FrameAssembler::update_missing_packets(uint16_t seq_num) {
    if (!newest_inserted_seq_num_)
        newest_inserted_seq_num_ = seq_num;

    const int kMaxPaddingAge = 1000;
    if (webrtc::AheadOf(seq_num, *newest_inserted_seq_num_)) {
        uint16_t old_seq_num = seq_num - kMaxPaddingAge;
        auto erase_to = missing_packets_.lower_bound(old_seq_num);
        missing_packets_.erase(missing_packets_.begin(), erase_to);

        if (webrtc::AheadOf(old_seq_num, *newest_inserted_seq_num_))
            *newest_inserted_seq_num_ = old_seq_num;

        ++*newest_inserted_seq_num_;
        while (webrtc::AheadOf(seq_num, *newest_inserted_seq_num_)) {
            missing_packets_.insert(*newest_inserted_seq_num_);
            ++*newest_inserted_seq_num_;
        }
    }
    else {
        missing_packets_.erase(seq_num);
    }
}

std::vector<VideoPacket> FrameAssembler::find_frames(uint16_t seq_num) {
    std::vector<VideoPacket> found_frames;
    for (size_t i = 0; i < buffer_.size() && potential_new_frame(seq_num); ++i) {
        size_t index = seq_num % buffer_.size();
        buffer_[index].continuous = true;
        if (buffer_[index].last_packet_in_frame.value()) {
            uint16_t start_seq_num = seq_num;
            size_t start_index = index;
            size_t tested_packets = 0;

            while (true) {
                ++tested_packets;
                if (buffer_[start_index].first_packet_in_frame.value()) {
                    break;
                }
                if (tested_packets == buffer_.size())
                    break;
                start_index = start_index > 0 ? start_index - 1 : buffer_.size() - 1;
                --start_seq_num;
            }

            if (!buffer_[index].key_frame.value() &&
                missing_packets_.upper_bound(start_seq_num) != missing_packets_.begin()) {
                return found_frames;
            }

            const uint16_t end_seq_num = seq_num + 1;
            uint16_t num_packets = end_seq_num - start_seq_num;
            found_frames.reserve(found_frames.size() + num_packets);
            for (uint16_t j = start_seq_num; j != end_seq_num; ++j) {
                VideoPacket packet = buffer_[j % buffer_.size()];
                found_frames.push_back(packet);
            }
            if (not found_frames.empty()) {
                assembled_frames_.push_back(std::move(found_frames));
            }

            missing_packets_.erase(missing_packets_.begin(), missing_packets_.upper_bound(seq_num));
        }
        ++seq_num;
    }
    return found_frames;
}

bool FrameAssembler::potential_new_frame(uint16_t seq_num) const {
    size_t index = seq_num % buffer_.size();
    size_t prev_index = index > 0 ? index - 1 : buffer_.size() - 1;
    const auto& entry = buffer_[index];
    const auto& prev_entry = buffer_[prev_index];

    if (entry.rtp.size() == 0)
        return false;
    if (entry.rtp.sequence_number() != seq_num)
        return false;
    if (entry.first_packet_in_frame.value())
        return true;
    if (prev_entry.rtp.size() == 0)
        return false;
    if (prev_entry.rtp.sequence_number() != static_cast<uint16_t>(entry.rtp.sequence_number() - 1))
        return false;
    if (prev_entry.rtp.timestamp() != entry.rtp.timestamp())
        return false;
    if (prev_entry.continuous)
        return true;

    return false;
}

VideoPacket::VideoPacket(const RtpPacket& rtp_packet) {
    LtFrameInfo frame_info{};
    if (rtp_packet.get_extension<LtFrameInfoExtension>(frame_info)) {
        encode_duration = frame_info.encode_duration();
        frame_id = frame_info.frame_id();
    }
    LtPacketInfo packet_info{};
    if (rtp_packet.get_extension<LtPacketInfoExtension>(packet_info)) {
        first_packet_in_frame = packet_info.is_first_packet_in_frame();
        last_packet_in_frame = packet_info.is_last_packet_in_frame();
        key_frame = packet_info.is_keyframe();
        retransmit = packet_info.is_retransmit();
        global_sequence_number = packet_info.sequence_number();
    }
    else {
        // PakcetInfo是必须的
        assert(false);
    }
}

} // namespace rtc2