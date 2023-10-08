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

#include <rtc2/rtc2.h>

#include <ltlib/logging.h>

namespace rtc2 {

Client::Client(const Params& params)
    : video_ssrc_{params.video_recv_ssrc}
    , audio_ssrc_{params.audio_recv_ssrc}
    , on_signaling_message_{params.on_signaling_message} {}

std::unique_ptr<Client> Client::create(const Params& params) {
    Connection::Params conn_params{};
    // audio
    Connection::AudioReceiveParams audio_recv_param{};
    audio_recv_param.ssrc = params.audio_recv_ssrc;
    audio_recv_param.on_audio_data = [on_audio = params.on_audio](const uint8_t* data,
                                                                  uint32_t size) {
        lt::AudioData audio_data{};
        audio_data.data = data;
        audio_data.size = size;
        on_audio(audio_data);
    };
    conn_params.receive_audio = {audio_recv_param};
    // video
    Connection::VideoReceiveParams video_recv_param{};
    video_recv_param.ssrc = params.video_recv_ssrc;
    video_recv_param.on_decodable_frame = [on_frame = params.on_video](VideoFrame frame) {
        lt::VideoFrame video_frame{};
        video_frame.is_keyframe = frame.is_keyframe;
        video_frame.ltframe_id = frame.frame_id;
        video_frame.data = frame.data;
        video_frame.size = frame.size;
        video_frame.start_encode_timestamp_us =
            frame.encode_timestamp_us; // 这个timestamp取编码前还是编码后比较合理？
        video_frame.end_encode_timestamp_us = frame.encode_duration_us + frame.encode_timestamp_us;
        on_frame(video_frame);
    };
    conn_params.receive_video = {video_recv_param};
    // data channel
    Connection::DataParams data_param{};
    data_param.ssrc = reliable_ssrc_;
    data_param.on_data = params.on_data;
    conn_params.data = data_param;
    // others
    conn_params.is_server = false;
    conn_params.key_and_cert = params.key_and_cert;
    conn_params.remote_digest = params.remote_digest;
    conn_params.on_signaling_message =
        [cb = params.on_signaling_message](const std::string& key, const std::string& value) {
            cb(key.c_str(), value.c_str());
        };
    //
    auto conn = Connection::create(conn_params);
    if (conn == nullptr) {
        return nullptr;
    }
    std::unique_ptr<Client> client{new Client{params}};
    client->conn_ = conn;
    return client;
}

// 跑在用户线程
bool Client::connect() {
    conn_->start();
    return true;
}

void Client::close() {}

bool Client::sendData(const uint8_t* data, uint32_t size, bool is_reliable) {
    if (is_reliable) {
        return conn_->sendData(reliable_ssrc_, data, size);
    }
    else {
        return conn_->sendData(half_reliable_ssrc_, data, size);
    }
}

void Client::onSignalingMessage(const char* key, const char* value) {
    conn_->onSignalingMessage(key, value);
}

//****************************************************************************

Server::Server(const Params& params)
    : video_ssrc_{params.video_send_ssrc}
    , audio_ssrc_{params.audio_send_ssrc}
    , on_signaling_message_{params.on_signaling_message} {}

std::unique_ptr<Server> Server::create(const Params& params) {
    Connection::Params conn_params{};
    // audio
    Connection::AudioSendParams audio_send_param{};
    audio_send_param.ssrc = params.audio_send_ssrc;
    conn_params.send_audio = {audio_send_param};
    // video
    Connection::VideoSendParams video_send_param{};
    video_send_param.ssrc = params.video_send_ssrc;
    video_send_param.on_bwe_update = params.on_video_bitrate_update;
    video_send_param.on_request_keyframe = params.on_keyframe_request;
    conn_params.send_video = {video_send_param};
    // data channel
    Connection::DataParams data_param{};
    data_param.ssrc = reliable_ssrc_;
    data_param.on_data = params.on_data;
    conn_params.data = data_param;
    // others
    conn_params.is_server = true;
    conn_params.key_and_cert = params.key_and_cert;
    conn_params.remote_digest = params.remote_digest;
    conn_params.on_signaling_message =
        [cb = params.on_signaling_message](const std::string& key, const std::string& value) {
            cb(key.c_str(), value.c_str());
        };
    //
    auto conn = Connection::create(conn_params);
    if (conn == nullptr) {
        return nullptr;
    }
    std::unique_ptr<Server> server{new Server{params}};
    server->conn_ = conn;
    return server;
}

void Server::close() {}

bool Server::sendData(const uint8_t* data, uint32_t size, bool is_reliable) {
    if (is_reliable) {
        return conn_->sendData(reliable_ssrc_, data, size);
    }
    else {
        return conn_->sendData(half_reliable_ssrc_, data, size);
    }
}

bool Server::sendAudio(const lt::AudioData& audio_data) {
    return conn_->sendAudio(audio_ssrc_, reinterpret_cast<const uint8_t*>(audio_data.data),
                            audio_data.size);
}

bool Server::sendVideo(const lt::VideoFrame& frame) {
    rtc2::VideoFrame video_frame{};
    video_frame.frame_id = frame.ltframe_id;
    video_frame.is_keyframe = frame.is_keyframe;
    // video_frame.encode_timestamp_us = frame.
    video_frame.encode_duration_us =
        frame.end_encode_timestamp_us - frame.start_encode_timestamp_us;
    video_frame.data = frame.data;
    video_frame.size = frame.size;
    return conn_->sendVideo(video_ssrc_, video_frame);
}

void Server::onSignalingMessage(const char* key, const char* value) {
    conn_->onSignalingMessage(key, value);
}

uint32_t Server::bwe() const {
    return 0;
}

uint32_t Server::nack() const {
    return 0;
}

} // namespace rtc2