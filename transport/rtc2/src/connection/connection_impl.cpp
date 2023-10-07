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

#include "connection_impl.h"

#include <cassert>

#include <regex>
#include <sstream>

#include <ltlib/logging.h>

namespace {

std::string SigEpInfo = "epinfo";
std::string FieldType = "type";
std::string FieldAddr = "addr";

enum class PacketType { Unknown, Rtp, Rtcp };

constexpr uint8_t kRtpVersion = 2;
constexpr size_t kMinRtcpPacketLen = 4;
constexpr size_t kMinRtpPacketLen = 12;

bool is_rtp(const uint8_t* data, uint32_t size) {

    if (size < kMinRtpPacketLen) {
        return false;
    }
    return data[0] >> 6 == kRtpVersion;
}

bool is_rtcp(const uint8_t* data, uint32_t size) {
    if (size < kMinRtcpPacketLen) {
        return false;
    }
    if (data[0] >> 6 != kRtpVersion) {
        return false;
    }
    uint8_t pt = data[1] & 0x7F;
    return (63 < pt) && (pt < 96);
}

PacketType infer_packet_type(const uint8_t* data, uint32_t size) {
    if (is_rtcp(data, size)) {
        return PacketType::Rtcp;
    }
    else if (is_rtp(data, size)) {
        return PacketType::Rtp;
    }
    else {
        return PacketType::Unknown;
    }
}

uint32_t changeEndian(uint32_t val) {
    return ((((val)&0xff000000) >> 24) | (((val)&0x00ff0000) >> 8) | (((val)&0x0000ff00) << 8) |
            (((val)&0x000000ff) << 24));
}

} // namespace

namespace rtc2 {

ConnectionImpl::ConnectionImpl(const Connection::Params& params)
    : params_{params} {}

ConnectionImpl::~ConnectionImpl() {}

bool ConnectionImpl::init() {
    send_thread_ = ltlib::TaskThread::create("rtc2_send");
    recv_thread_ = ltlib::TaskThread::create("rtc2_recv");

    // network channel
    NetworkChannel::Params net_param{};
    net_param.is_server = params_.is_server;
    net_param.username = params_.p2p_username;
    net_param.password = params_.p2p_password;
    if (!params_.stun_addr.empty()) {
        net_param.stun = Address::from_str(params_.stun_addr);
        if (net_param.stun.family() == -1) {
            LOG(ERR) << "Invalid stun addr";
            return false;
        }
    }
    if (!params_.relay_addr.empty()) {
        if (params_.relay_addr.size() < 8 || params_.relay_addr.substr(0, 6) != "relay:") {
            LOG(ERR) << "Invalid relay addr";
            return false;
        }
        std::regex pattern = std::regex{"relay:(.+?:[0-9]+?):(.+?):(.+?)"};
        std::smatch sm;
        if (!std::regex_match(params_.relay_addr, sm, pattern)) {
            LOG(ERR) << "Invalid relay addr";
            return false;
        }
        net_param.relay = Address::from_str(sm[1]);
        net_param.relay_username = sm[2];
        net_param.relay_password = sm[3];
    }
    // NetworkChannel的on_error和on_info是在这里设置，on_read和on_connected是在DTLS设置
    net_param.on_endpoint_info_gathered =
        std::bind(&ConnectionImpl::onEndpointInfo, this, std::placeholders::_1);
    net_param.on_error = std::bind(&ConnectionImpl::onNetError, this, std::placeholders ::_1);
    network_channel_ = NetworkChannel::create(net_param); // 内含线程

    // pacer
    Pacer::Params pacer_param{};
    pacer_param.post_task =
        std::bind(&NetworkChannel::post, network_channel_.get(), std::placeholders::_1);
    pacer_param.post_delayed_task = std::bind(&NetworkChannel::postDelay, network_channel_.get(),
                                              std::placeholders::_1, std::placeholders::_2);
    pacer_ = std::make_shared<Pacer>(pacer_param);

    // media stream
    for (auto& p : params_.send_video) {
        VideoSendStream::Params param{};
        param.ssrc = p.ssrc;
        param.on_request_keyframe = param.on_request_keyframe;
        param.pacer = pacer_.get();
        video_send_streams_.push_back(std::make_shared<VideoSendStream>(param));
    }
    for (auto& p : params_.receive_video) {
        VideoReceiveStream::Params param{};
        param.ssrc = p.ssrc;
        video_receive_streams_.push_back(std::make_shared<VideoReceiveStream>(param));
    }
    for (auto& p : params_.send_audio) {
        AudioSendStream::Params param{};
        param.ssrc = p.ssrc;
        param.pacer = pacer_.get();
        audio_send_streams_.push_back(std::make_shared<AudioSendStream>(param));
    }
    for (auto& p : params_.receive_audio) {
        AudioReceiveStream::Params param{};
        param.ssrc = p.ssrc;
        audio_receive_streams_.push_back(std::make_shared<AudioReceiveStream>(param));
    }

    // message channel
    MessageChannel::Params msg_param{};
    msg_param.network_channel = network_channel_.get();
    msg_param.reliable_ssrc = 0;
    msg_param.half_reliable_ssrc = 0;
    msg_param.mtu = 1400;
    msg_param.sndwnd = 128;
    msg_param.rcvwnd = 128;
    message_channel_ = std::make_shared<MessageChannel>(msg_param);

    // dtls channel
    DtlsChannel::Params dtls_params{};
    dtls_params.is_server = params_.is_server;
    dtls_params.key_and_cert = params_.key_and_cert;
    dtls_params.remote_digest = params_.remote_digest;
    dtls_params.network_channel = network_channel_.get();
    dtls_params.on_read_packet =
        std::bind(&ConnectionImpl::onDtlsPacket, this, std::placeholders::_1, std::placeholders::_2,
                  std::placeholders::_3);
    dtls_params.on_read_rtp_packet =
        std::bind(&ConnectionImpl::onRtpRtcpPacket, this, std::placeholders::_1,
                  std::placeholders::_2, std::placeholders::_3);
    dtls_params.on_connected = std::bind(&ConnectionImpl::onDtlsConnected, this);
    dtls_params.on_disconnected = std::bind(&ConnectionImpl::onDtlsDisconnected, this);
    dtls_ = DtlsChannel::create(dtls_params);
    if (dtls_ == nullptr) {
        return false;
    }
    if (params_.is_server) {
        network_channel_->start();
    }
    return true;
}

void ConnectionImpl::start() {
    network_channel_->start();
}

bool ConnectionImpl::sendData(const uint8_t* data, uint32_t size) {
    return message_channel_->sendMessage(data, size, true);
}

bool ConnectionImpl::sendVideo(uint32_t ssrc, const VideoFrame& frame) {
    for (auto& stream : video_send_streams_) {
        if (stream->ssrc() == ssrc) {
            stream->sendFrame(frame);
            return true;
        }
    }
    return false;
}

bool ConnectionImpl::sendAudio(uint32_t ssrc, const uint8_t* data, uint32_t size) {
    for (auto& stream : audio_send_streams_) {
        if (stream->ssrc() == ssrc) {
            stream->send(data, size);
            return true;
        }
    }
    return false;
}

void ConnectionImpl::onSignalingMessage(const std::string& key, const std::string& value) {
    if (key != SigEpInfo) {
        LOG(ERR) << "Received unknown signaling message key:" << key;
        return;
    }
    std::istringstream iss(value);
    std::string key1, key2, type, addr;
    iss >> key1;
    iss >> type;
    iss >> key2;
    iss >> addr;
    if (key1 != FieldType || key2 != FieldAddr || type.empty() || addr.empty()) {
        LOG(ERR) << "Invalid signaling message: " << value;
        return;
    }
    EndpointInfo info{};
    info.type = from_str<EndpointType>(type);
    if (info.type == EndpointType::Unknown) {
        LOG(ERR) << "Unknown EndpointType " << type;
        return;
    }
    info.address = Address::from_str(addr);
    if (info.address.family() != AF_INET) {
        LOG(ERR) << "Invalid address " << addr;
        return;
    }
    network_channel_->addRemoteInfo(info);
}

void ConnectionImpl::onRtpRtcpPacket(const uint8_t* data, uint32_t size, int64_t time_us) {
    PacketType pkt = infer_packet_type(data, size);
    switch (pkt) {
    case PacketType::Rtp:
        onRtpPacket(data, size, time_us);
        break;
    case PacketType::Rtcp:
        onRtcpPacket(data, size, time_us);
        break;
    case PacketType::Unknown:
    default:
        assert(false);
        LOG(DEBUG) << "Received unknown packet type " << (int)pkt;
        break;
    }
}

void ConnectionImpl::onRtpPacket(const uint8_t* data, uint32_t size, int64_t time_us) {
    uint32_t ssrc = *(uint32_t*)(data + 8);
    ssrc = changeEndian(ssrc);
    for (auto& stream : video_receive_streams_) {
        if (stream->ssrc() == ssrc) {
            stream->onRtpPacket(data, size, time_us);
            return;
        }
    }
    for (auto& stream : audio_receive_streams_) {
        if (stream->ssrc() == ssrc) {
            stream->onRtpPacket(data, size, time_us);
            return;
        }
    }
}

void ConnectionImpl::onRtcpPacket(const uint8_t* data, uint32_t size, int64_t time_us) {
    uint32_t ssrc = *(uint32_t*)(data + 8);
    ssrc = changeEndian(ssrc);
    for (auto& stream : video_send_streams_) {
        if (stream->ssrc() == ssrc) {
            stream->onRtcpPacket(data, size, time_us);
            return;
        }
    }
    for (auto& stream : video_receive_streams_) {
        if (stream->ssrc() == ssrc) {
            stream->onRtcpPacket(data, size, time_us);
            return;
        }
    }
    for (auto& stream : audio_send_streams_) {
        if (stream->ssrc() == ssrc) {
            stream->onRtcpPacket(data, size, time_us);
            return;
        }
    }
    for (auto& stream : audio_receive_streams_) {
        if (stream->ssrc() == ssrc) {
            stream->onRtcpPacket(data, size, time_us);
            return;
        }
    }
}

void ConnectionImpl::onDtlsPacket(const uint8_t* data, uint32_t size, int64_t time_us) {
    message_channel_->onRecvData(data, size, time_us);
}

void ConnectionImpl::onDtlsConnected() {
    LOG(INFO) << "Connected";
}

void ConnectionImpl::onDtlsDisconnected() {
    LOG(INFO) << "Disconnected";
}

// 原本让EndpointInfo回调到上层，让上层按照自己的方式做序列化会更好
// 但具体到这个项目，lanthing和rtc2是一体的，你我之间不必拿刻度尺分太清，怎么方便怎么来
void ConnectionImpl::onEndpointInfo(const EndpointInfo& info) {
    std::ostringstream oss;
    oss << FieldType << " " << to_str(info.type) << " " << FieldAddr << " "
        << info.address.to_string();
    params_.on_signaling_message(SigEpInfo, oss.str());
}

void ConnectionImpl::onNetError(int32_t error) {
    (void)error;
}

} // namespace rtc2