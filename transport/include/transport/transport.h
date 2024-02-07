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
#include <cstring>

#if defined(LT_WINDOWS)
#if defined(BUILDING_LT_EXE)
#define TP_API _declspec(dllimport)
#else
#define TP_API _declspec(dllexport)
#endif
#else
#define TP_API
#endif

namespace lt {

enum class VideoCodecType : uint32_t {
    Unknown = 0,
    H264_420 = 0b0000'0001,
    H265_420 = 0b0000'0010,
    H264_444 = 0b0000'0100,
    H265_444 = 0b0000'1000,
    AV1 = 0b0001'0000,
    H264_420_SOFT = 0b0010'0000,
    H264 = H264_420,
    H265 = H265_420,
};

inline constexpr uint32_t operator&(uint32_t value, VideoCodecType codec) {
    return value & static_cast<uint32_t>(codec);
}

inline constexpr uint32_t operator&(VideoCodecType codec, uint32_t value) {
    return value & codec;
}

inline constexpr uint32_t operator&(VideoCodecType left, VideoCodecType right) {
    return static_cast<uint32_t>(left) & static_cast<uint32_t>(right);
}

inline constexpr uint32_t operator|(uint32_t value, VideoCodecType codec) {
    return value | static_cast<uint32_t>(codec);
}

inline constexpr uint32_t operator|(VideoCodecType codec, uint32_t value) {
    return value | codec;
}

inline constexpr uint32_t operator|(VideoCodecType left, VideoCodecType right) {
    return static_cast<uint32_t>(left) | static_cast<uint32_t>(right);
}

inline constexpr bool isYUV444(VideoCodecType ct) {
    return (ct == VideoCodecType::H264_444 || ct == VideoCodecType::H265_444);
}

inline constexpr bool isYUV420(VideoCodecType ct) {
    return (ct == VideoCodecType::H264_420 || ct == VideoCodecType::H265_420 ||
            ct == VideoCodecType::H264_420_SOFT);
}

inline constexpr bool isHard(VideoCodecType ct) {
    return (ct == VideoCodecType::H264_420 || ct == VideoCodecType::H265_420 ||
            ct == VideoCodecType::H264_444 || ct == VideoCodecType::H265_444);
}

inline constexpr bool isSoft(VideoCodecType ct) {
    return ct == VideoCodecType::H264_420_SOFT;
}

inline constexpr bool isAVC(VideoCodecType ct) {
    if (ct == VideoCodecType::H264_420 || ct == VideoCodecType::H264_444 ||
        ct == VideoCodecType::H264_420_SOFT) {
        return true;
    }
    else {
        return false;
    }
}

inline constexpr bool isHEVC(VideoCodecType ct) {
    if (ct == VideoCodecType::H265_420 || ct == VideoCodecType::H265_444) {
        return true;
    }
    else {
        return false;
    }
}

inline constexpr const char* toString(VideoCodecType type) {
    switch (type) {
    case VideoCodecType::H264_420:
        return "AVC";
    case VideoCodecType::H265_420:
        return "HEVC";
    case VideoCodecType::H264_444:
        return "AVC444";
    case VideoCodecType::H265_444:
        return "HEVC444";
    case VideoCodecType::AV1:
        return "AV1";
    case VideoCodecType::H264_420_SOFT:
        return "AVC_SOFT";
    default:
        return "?";
    }
}

inline VideoCodecType videoCodecType(const char* type) {
    if (std::strcmp(type, "AVC") == 0) {
        return VideoCodecType::H264_420;
    }
    else if (std::strcmp(type, "HEVC") == 0) {
        return VideoCodecType::H265_420;
    }
    else if (std::strcmp(type, "AVC444") == 0) {
        return VideoCodecType::H264_444;
    }
    else if (std::strcmp(type, "HEVC444") == 0) {
        return VideoCodecType::H265_444;
    }
    else if (std::strcmp(type, "AV1") == 0) {
        return VideoCodecType::AV1;
    }
    else if (std::strcmp(type, "AVC_SOFT") == 0) {
        return VideoCodecType::H264_420_SOFT;
    }
    else {
        return VideoCodecType::Unknown;
    }
}

enum class AudioCodecType { Unknown, PCM, OPUS };

enum class LinkType {
    Unknown = 0,
    UDP = 1,
    LanUDP,
    WanUDP,
    IPv6UDP,
    RelayUDP,
    TCP = 11,
};

constexpr const char* toString(LinkType type) {
    switch (type) {
    case LinkType::UDP:
        return "UDP";
    case LinkType::LanUDP:
        return "LanUDP";
    case LinkType::WanUDP:
        return "WanUDP";
    case LinkType::IPv6UDP:
        return "IPv6UDP";
    case LinkType::RelayUDP:
        return "RelayUDP";
    case LinkType::TCP:
        return "TCP";
    default:
        return "?";
    }
}

struct TP_API VideoFrame {
    bool is_keyframe;
    uint64_t ltframe_id;
    const uint8_t* data;
    uint32_t size;
    uint32_t width;
    uint32_t height;
    int64_t capture_timestamp_us;
    int64_t start_encode_timestamp_us;
    int64_t end_encode_timestamp_us;
};

struct TP_API AudioData {
    const void* data;
    uint32_t size;
};

namespace tp { // transport

typedef void (*OnData)(void*, const uint8_t*, uint32_t, bool);
typedef void (*OnVideo)(void*, const VideoFrame&);
typedef void (*OnAudio)(void*, const AudioData&);
typedef void (*OnConnected)(void*, LinkType);
typedef void (*OnConnChanged)(void*, LinkType /*old*/, LinkType /*new*/);
typedef void (*OnDisconnected)(void*);
typedef void (*OnFailed)(void*);
typedef void (*OnSignalingMessage)(void*, const char*, const char*);
typedef void (*OnKeyframeRequest)(void*);
typedef void (*OnVEncoderBitrateUpdate)(void*, uint32_t bps);
typedef void (*OnLossRateUpdate)(void*, float);
typedef void (*OnTransportStat)(void*, uint32_t /*bwe_bps*/, uint32_t /*nack*/);

class TP_API Client {
public:
    virtual ~Client() {}
    virtual bool connect() = 0;
    virtual void close() = 0;
    virtual bool sendData(const uint8_t* data, uint32_t size, bool is_reliable) = 0;
    virtual void onSignalingMessage(const char* key, const char* value) = 0;
};

class TP_API Server {
public:
    virtual ~Server() {}
    virtual void close() = 0;
    virtual bool sendData(const uint8_t* data, uint32_t size, bool is_reliable) = 0;
    virtual bool sendAudio(const AudioData& audio_data) = 0;
    virtual bool sendVideo(const VideoFrame& frame) = 0;
    virtual void onSignalingMessage(const char* key, const char* value) = 0;
};

} // namespace tp

} // namespace lt