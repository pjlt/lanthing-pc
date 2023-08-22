#pragma once
#include <cstdint>
#include <functional>
#include <optional>
#include <string>

namespace lt {

enum class VideoCodecType { Unknown, H264, H265 };

struct VideoFrame {
    bool is_keyframe;
    uint64_t ltframe_id;
    const uint8_t* data;
    uint32_t size;
    uint32_t width;
    uint32_t height;
    int64_t capture_timestamp_us;
    int64_t start_encode_timestamp_us;
    int64_t end_encode_timestamp_us;
    std::optional<uint32_t> temporal_id;
};

namespace tp { // transport

using OnData = std::function<void(const uint8_t*, uint32_t, bool)>;
using OnVideo = std::function<void(const VideoFrame&)>;
using OnAudio =
    std::function<void(uint32_t bits_per_sample, uint32_t sample_rate, uint32_t number_of_channels,
                       const void* audio_data, uint32_t size)>;
using OnConnected = std::function<void(/*connection info*/)>;
using OnConnChanged = std::function<void(/*1. old_conn_info, 2. new_conn_info*/)>;
using OnDisconnected = std::function<void()>;
using OnFailed = std::function<void()>;
using OnSignalingMessage = std::function<void(const char*, const char*)>;

class Client {
public:
    virtual ~Client() {}
    virtual bool connect() = 0;
    virtual void close() = 0;
    virtual bool sendData(const uint8_t* data, uint32_t size, bool is_reliable) = 0;
    virtual void onSignalingMessage(const char* key, const char* value) = 0;
};

class Server {
public:
    virtual ~Server() {}
    virtual void close() = 0;
    virtual bool sendData(const uint8_t* data, uint32_t size, bool is_reliable) = 0;
    virtual bool sendAudio(const uint8_t* data, uint32_t size) = 0;
    virtual bool sendVideo(const VideoFrame& frame) = 0;
    virtual void onSignalingMessage(const char* key, const char* value) = 0;
};

} // namespace tp

} // namespace lt