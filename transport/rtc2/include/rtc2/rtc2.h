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

#include <transport/transport.h>

#include <rtc2/exports.h>

#include <rtc2/connection.h>

#include <rtc2/video_frame.h>

#include <rtc2/key_and_cert.h>

#include <rtc2/codec_types.h>

// rtc2很多代码是从旧项目(brtc/bco)凑出来的，代码风格会和lanthing不统一，等能run了再整理
// 这两个Client和Server是adapter，作为lanthing和rtc2的桥梁

// TODO: 头文件不包含STL，只用C接口

namespace rtc2 {

class RTC2_API Client : public lt::tp::Client {
public:
    struct Params {
        void* user_data;
        uint32_t audio_sample_rate;
        uint32_t audio_channels;
        uint32_t audio_recv_ssrc;
        uint32_t video_recv_ssrc;
        lt::tp::OnData on_data;
        lt::tp::OnVideo on_video;
        lt::tp::OnAudio on_audio;
        lt::tp::OnConnected on_connected;
        lt::tp::OnConnChanged on_conn_changed;
        lt::tp::OnFailed on_failed;
        lt::tp::OnDisconnected on_disconnected;
        lt::tp::OnSignalingMessage on_signaling_message;

        std::shared_ptr<KeyAndCert> key_and_cert;
        std::vector<uint8_t> remote_digest;
    };

public:
    static std::unique_ptr<Client> create(const Params& config);

    bool connect() override;
    void close() override;
    bool sendData(const uint8_t* data, uint32_t size, bool is_reliable) override;
    void onSignalingMessage(const char* key, const char* value) override;

private:
    Client(const Params& params);

private:
    std::shared_ptr<rtc2::Connection> conn_;
    static const uint32_t reliable_ssrc_ = 0x33445566;
    static const uint32_t half_reliable_ssrc_ = 0x44556677;
    uint32_t video_ssrc_;
    uint32_t audio_ssrc_;
    lt::tp::OnSignalingMessage on_signaling_message_;
};

class RTC2_API Server : public lt::tp::Server {
public:
    struct Params {
        void* user_data;
        uint32_t audio_sample_rate;
        uint32_t audio_channels;
        uint32_t video_send_ssrc;
        uint32_t audio_send_ssrc;
        lt::VideoCodecType video_codec_type;
        lt::tp::OnData on_data;
        lt::tp::OnConnected on_accepted;
        lt::tp::OnConnChanged on_conn_changed;
        lt::tp::OnFailed on_failed;
        lt::tp::OnDisconnected on_disconnected;
        lt::tp::OnSignalingMessage on_signaling_message;
        lt::tp::OnKeyframeRequest on_keyframe_request;
        lt::tp::OnVEncoderBitrateUpdate on_video_bitrate_update;
        lt::tp::OnLossRateUpdate on_loss_rate_update;
        std::shared_ptr<KeyAndCert> key_and_cert;
        std::vector<uint8_t> remote_digest;
    };

public:
    static std::unique_ptr<Server> create(const Params& config);

    void close() override;
    bool sendData(const uint8_t* data, uint32_t size, bool is_reliable) override;
    bool sendAudio(const lt::AudioData& audio_data) override;
    bool sendVideo(const lt::VideoFrame& frame) override;
    void onSignalingMessage(const char* key, const char* value) override;
    uint32_t bwe() const;
    uint32_t nack() const;

private:
    Server(const Params& params);

private:
    std::shared_ptr<rtc2::Connection> conn_;
    static const uint32_t reliable_ssrc_ = 0x33445566;
    static const uint32_t half_reliable_ssrc_ = 0x44556677;
    uint32_t video_ssrc_;
    uint32_t audio_ssrc_;
    lt::tp::OnSignalingMessage on_signaling_message_;
};

} // namespace rtc2