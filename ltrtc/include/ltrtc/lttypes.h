#pragma once
#include <cstdint>
#include <string>
#include <memory>
#include <functional>
#include <optional>

namespace ltrtc
{

enum class VideoCodecType
{
    Unknown,
    H264,
    H265
};

struct VideoFrame
{
    bool is_keyframe;
    uint64_t ltframe_id;
    std::shared_ptr<uint8_t> data;
    uint32_t size;
    uint32_t width;
    uint32_t height;
    int64_t capture_timestamp_us;
    int64_t start_encode_timestamp_us;
    int64_t end_encode_timestamp_us;
    std::optional<uint32_t> temporal_id;
};

using OnData = std::function<void(const std::shared_ptr<uint8_t>&, uint32_t, bool)>;
using OnVideo = std::function<void(const VideoFrame&)>;
using OnAudio = std::function<void(uint32_t bits_per_sample, uint32_t sample_rate, uint32_t number_of_channels, const void* audio_data, uint32_t size)>;
using OnConnected = std::function<void(/*connection info*/)>;
using OnConnChanged = std::function<void(/*1. old_conn_info, 2. new_conn_info*/)>;
using OnDisconnected = std::function<void()>;
using OnFailed = std::function<void()>;
using OnSignalingMessage = std::function<void(const std::string&, const std::string&)>;

} // namespace ltrtc