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
#include <mutex>

#include <ltlib/threads.h>

#include <rtc2/connection.h>
#include <rtc2/video_frame.h>

#include <modules/cc/pacer.h>
#include <modules/dtls/dtls_channel.h>
#include <modules/network/network_channel.h>
#include <stream/audio_receive_stream.h>
#include <stream/audio_send_stream.h>
#include <stream/message_channel.h>
#include <stream/video_receive_stream.h>
#include <stream/video_send_stream.h>

namespace rtc2 {

class ConnectionImpl {
public:
    ConnectionImpl(const Connection::Params& params);
    ~ConnectionImpl();
    bool init();
    void start();
    bool sendData(const uint8_t* data, uint32_t size);
    bool sendVideo(uint32_t ssrc, const VideoFrame& frame);
    bool sendAudio(uint32_t ssrc, const uint8_t* data, uint32_t size);
    void onSignalingMessage(const std::string& key, const std::string& value);

private:
    void onRtpRtcpPacket(const uint8_t* data, uint32_t size, int64_t time_us);
    void onRtpPacket(const uint8_t* data, uint32_t size, int64_t time_us);
    void onRtcpPacket(const uint8_t* data, uint32_t size, int64_t time_us);

    void onDtlsPacket(const uint8_t* data, uint32_t size, int64_t time_us);
    void onDtlsConnected();
    void onDtlsDisconnected();

    void onEndpointInfo(const EndpointInfo& info);
    void onNetError(int32_t error);

private:
    Connection::Params params_;
    std::unique_ptr<ltlib::TaskThread> send_thread_;
    std::unique_ptr<ltlib::TaskThread> recv_thread_;
    std::unique_ptr<NetworkChannel> network_channel_; // 内含线程
    std::shared_ptr<Pacer> pacer_;
    std::mutex mutex_;
    std::vector<std::shared_ptr<VideoSendStream>> video_send_streams_;
    std::vector<std::shared_ptr<VideoReceiveStream>> video_receive_streams_;
    std::vector<std::shared_ptr<AudioSendStream>> audio_send_streams_;
    std::vector<std::shared_ptr<AudioReceiveStream>> audio_receive_streams_;
    std::shared_ptr<MessageChannel> message_channel_;
    std::shared_ptr<DtlsChannel> dtls_;
    std::atomic<bool> started_ = false;
};

} // namespace rtc2