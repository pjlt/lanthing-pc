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

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <rtc2/exports.h>
#include <rtc2/key_and_cert.h>
#include <rtc2/video_frame.h>

namespace rtc2 {

class ConnectionImpl;
class RTC2_API Connection {
public:
    struct RTC2_API VideoSendParams {
        uint32_t ssrc;
        std::function<void(uint32_t bps)> on_bwe_update; // 分配给这条视频流的带宽
        std::function<void()> on_request_keyframe;
    };

    struct RTC2_API VideoReceiveParams {
        uint32_t ssrc;
        std::function<void(VideoFrame)> on_decodable_frame;
    };

    struct RTC2_API AudioSendParams {
        uint32_t ssrc;
    };

    struct RTC2_API AudioReceiveParams {
        uint32_t ssrc;
        std::function<void(const uint8_t* data, uint32_t size)> on_audio_data;
    };

    struct RTC2_API DataParams {
        uint32_t ssrc;
        std::function<void(const uint8_t* data, uint32_t size, bool reliable)> on_data;
    };

    struct RTC2_API Params {
        // 两端收发的ssrc必须匹配一致，由使用者在自己的业务层协商
        std::vector<VideoSendParams> send_video;
        std::vector<VideoReceiveParams> receive_video;
        std::vector<AudioSendParams> send_audio;
        std::vector<AudioReceiveParams> receive_audio;
        DataParams data;

        bool is_server;
        std::shared_ptr<KeyAndCert> key_and_cert;
        std::vector<uint8_t> remote_digest;
        std::string p2p_username;
        std::string p2p_password;
        std::string stun_addr;
        std::string relay_addr;

        std::function<void(const std::string& key, const std::string& value)> on_signaling_message;
    };

public:
    static std::shared_ptr<Connection> create(const Params& params);
    void start();
    bool sendData(uint32_t ssrc, const uint8_t* data, uint32_t size);
    bool sendVideo(uint32_t ssrc, const VideoFrame& frame);
    bool sendAudio(uint32_t ssrc, const uint8_t* data, uint32_t size);
    void onSignalingMessage(const std::string& key, const std::string& value);

private:
    Connection() = default;

private:
    std::shared_ptr<ConnectionImpl> impl_;
};

} // namespace rtc2