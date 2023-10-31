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

enum class VideoCodecType { Unknown, H264, H265 };

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
typedef void (*OnConnChanged)(void* /*1. old_conn_info, 2. new_conn_info*/);
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