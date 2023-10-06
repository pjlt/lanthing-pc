/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#pragma once
#include <map>
#include <memory>
#include <optional>
#include <queue>
#include <set>

#include <modules/rtp/rtp_packet.h>
#include <modules/sequence_number_util.h>

namespace rtc2 {

struct VideoPacket {
    VideoPacket() = default;
    VideoPacket(const RtpPacket& rtp_packet);
    RtpPacket rtp;
    bool continuous = false;
    std::optional<bool> first_packet_in_frame;
    std::optional<bool> last_packet_in_frame;
    std::optional<bool> key_frame;
    std::optional<bool> retransmit;
    std::optional<uint16_t> global_sequence_number;
    std::optional<uint16_t> frame_id;
    std::optional<uint16_t> encode_duration;
};

class FrameAssembler {
public:
    struct InsertResult {
        std::vector<VideoPacket> packets;
        bool buffer_cleared = false;
    };

public:
    FrameAssembler(size_t start_size, size_t max_size);
    InsertResult insert(const VideoPacket& packet);

private:
    bool expand_buffer();
    void clear_internal();
    void update_missing_packets(uint16_t seq_num);
    std::vector<VideoPacket> find_frames(uint16_t seq_num);
    bool potential_new_frame(uint16_t seq_num) const;

private:
    std::vector<VideoPacket> buffer_;
    const size_t max_size_;
    bool first_packet_received_ = false;
    uint16_t first_seq_num_ = 0;
    bool is_cleared_to_first_seq_num_ = false;
    std::optional<int64_t> last_received_packet_ms_;
    std::set<uint16_t, webrtc::DescendingSeqNumComp<uint16_t>> missing_packets_;
    std::optional<uint16_t> newest_inserted_seq_num_;
    std::deque<std::vector<VideoPacket>> assembled_frames_;
    // webrtc::SeqNumUnwrapper<uint16_t> packet_id_unwrapper_;
    // webrtc::SeqNumUnwrapper<uint16_t> frame_id_unwrapper_;
    // std::map<int64_t, Frame> frames_;
};

} // namespace rtc2