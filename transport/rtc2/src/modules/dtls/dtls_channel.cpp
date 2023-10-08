/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2021-2023 Zhennan Tu <zhennan.tu@gmail.com>
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

#include "dtls_channel.h"

#include <cassert>

#include <span>

#include <ltlib/logging.h>

#include <ltlib/times.h>

namespace {

const size_t kDtlsRecordHeaderLen = 13;
const size_t kMaxDtlsPacketLen = 2048;
const size_t kMinRtpPacketLen = 12;

bool IsDtlsPacket(const uint8_t* data, size_t len) {
    return (len >= kDtlsRecordHeaderLen && (data[0] > 19 && data[0] < 64));
}

bool IsDtlsClientHelloPacket(const uint8_t* data, size_t len) {
    if (!IsDtlsPacket(data, len)) {
        return false;
    }
    const uint8_t* u = reinterpret_cast<const uint8_t*>(data);
    return len > 17 && u[0] == 22 && u[13] == 1;
}

bool IsRtpPacket(const uint8_t* data, size_t len) {
    return (len >= kMinRtpPacketLen && (data[0] & 0xC0) == 0x80);
}

} // namespace

namespace rtc2 {
std::unique_ptr<DtlsChannel> DtlsChannel::create(const Params& params) {
    std::unique_ptr<DtlsChannel> dtls{new DtlsChannel{params}};
    if (!dtls->init()) {
        return nullptr;
    }
    return dtls;
}
DtlsChannel::DtlsChannel(const Params& params)
    : is_server_{params.is_server}
    , key_and_cert_{params.key_and_cert}
    , remote_digest_{params.remote_digest}
    , network_channel_{params.network_channel}
    , on_read_packet_{params.on_read_packet}
    , on_read_rtp_packet_{params.on_read_rtp_packet}
    , on_connected_{params.on_connected}
    , on_disconnected_{params.on_disconnected} {
    network_channel_->setOnRead(std::bind(&DtlsChannel::onReadNetPacket, this,
                                          std::placeholders::_1, std::placeholders::_2,
                                          std::placeholders::_3));
    network_channel_->setOnConnChanged(std::bind(&DtlsChannel::onNetworkConnected, this,
                                                 std::placeholders::_1, std::placeholders::_2,
                                                 std::placeholders::_3));
}

bool DtlsChannel::init() {
    MbedDtls::Params params{};
    params.is_server = is_server_;
    params.key_and_cert = key_and_cert_;
    params.peer_digest = remote_digest_;
    params.write_to_network =
        std::bind(&DtlsChannel::writeToNetwork, this, std::placeholders::_1, std::placeholders::_2);
    params.on_receive = std::bind(&DtlsChannel::onDecryptedPacket, this, std::placeholders::_1,
                                  std::placeholders::_2);
    params.on_handshake_done =
        std::bind(&DtlsChannel::onHandshakeDone, this, std::placeholders ::_1);
    params.on_eof = std::bind(&DtlsChannel::onDtlsEOF, this);
    params.on_tls_error = std::bind(&DtlsChannel::onDtlsError, this);
    mbed_ = MbedDtls::create(params);
    if (mbed_ == nullptr) {
        return false;
    }
    return true;
}

void DtlsChannel::startHandshake() {
    if (dtls_state() != DtlsState::New && dtls_state() != DtlsState::Connecting) {
        return;
    }
    if (!mbed_->startHandshake()) {
        dtls_state_ = DtlsState::Failed;
        return;
    }
    dtls_state_ = DtlsState::Connecting;
    network_channel_->postDelay(50, std::bind(&DtlsChannel::startHandshake, this));
}

void DtlsChannel::onHandshakeDone(bool success) {
    if (success) {
        dtls_state_ = DtlsState::Connected;
        on_connected_();
    }
    else {
        dtls_state_ = DtlsState::Failed;
        on_disconnected_();
    }
}

void DtlsChannel::writeToNetwork(const uint8_t* data, uint32_t size) {
    std::span<const uint8_t> span{data, data + size};
    network_channel_->sendPacket({span});
}

void DtlsChannel::onDecryptedPacket(const uint8_t* data, uint32_t size) {
    on_read_packet_(data, size, ltlib::steady_now_us());
}

void DtlsChannel::onDtlsEOF() {
    dtls_state_ = DtlsState::Closed;
    on_disconnected_();
}

void DtlsChannel::onDtlsError() {
    dtls_state_ = DtlsState::Failed;
    on_disconnected_();
}

DtlsChannel::~DtlsChannel() {}

DtlsState DtlsChannel::dtls_state() const {
    return dtls_state_;
}

int DtlsChannel::sendPacket(const uint8_t* data, uint32_t size, bool bypass) {

    switch (dtls_state()) {
    case DtlsState::Connected:
        if (bypass) {
            assert(!srtp_ciphers_.empty());
            if (!IsRtpPacket(data, size)) {
                return -1;
            }
            std::span<const uint8_t> span{data, data + size};
            return network_channel_->sendPacket({span});
        }
        else {
            return mbed_->send(data, size) ? static_cast<int>(size) : -1;
        }
    default:
        LOG(WARNING) << "sendPacket while dtls_state==" << (int)dtls_state();
        return -1;
    }
}

bool DtlsChannel::checkAndHandleDtlsPacket(const uint8_t* data, uint32_t size) {
    const uint8_t* tmp_data = reinterpret_cast<const uint8_t*>(data);
    uint32_t tmp_size = size;
    while (tmp_size > 0) {
        if (tmp_size < kDtlsRecordHeaderLen)
            return false;

        uint32_t record_len = (tmp_data[11] << 8) | (tmp_data[12]);
        if ((record_len + kDtlsRecordHeaderLen) > tmp_size)
            return false;

        tmp_data += record_len + kDtlsRecordHeaderLen;
        tmp_size -= record_len + kDtlsRecordHeaderLen;
    }
    return mbed_->onNetworkData(data, size);
}

void DtlsChannel::onNetworkConnected(const EndpointInfo& local, const EndpointInfo& remote,
                                     int64_t used_time_ms) {
    (void)used_time_ms;
    (void)local;
    (void)remote;
    if (network_connected_) {
        LOG(INFO) << "Underlying network changed";
        return;
    }
    network_connected_ = true;
    switch (dtls_state()) {
    case DtlsState::New:
        startHandshake();
        break;
    default:
        // 当前P2P实现只会调一次onNetworkConnected
        LOG(WARNING) << "onNetworkConnected() while state==" << (int)dtls_state();
        break;
    }
}

void DtlsChannel::onReadNetPacket(const uint8_t* data, uint32_t size, int64_t time_us) {
    switch (dtls_state()) {
    case DtlsState::New:
        // 没有收到onConnected事件却收到包，说明写bug了。暂时保留WebRTC的逻辑，后面再做错误处理
        LOG(WARNING) << "Packet received before DTLS started.";

        if (IsDtlsClientHelloPacket(data, size)) {
            LOG(ERR) << "Received DTLS ClientHello before connected";
        }
        else {
            LOG(INFO) << "Received unknown packet before connected";
        }
        break;

    case DtlsState::Connecting:
    case DtlsState::Connected:
        if (IsDtlsPacket(data, size)) {
            if (!checkAndHandleDtlsPacket(data, size)) {
                LOG(WARNING) << "Failed to handle DTLS packet.";
                return;
            }
        }
        else {
            if (dtls_state() != DtlsState::Connected) {
                LOG(WARNING) << "Received non-DTLS packet before DTLS "
                                "complete.";
                return;
            }

            if (!IsRtpPacket(data, size)) {
                LOG(WARNING) << "Received unexpected non-DTLS packet.";
                return;
            }

            on_read_rtp_packet_(data, size, time_us);
        }
        break;
    case DtlsState::Failed:
    case DtlsState::Closed:
    default:
        break;
    }
}

} // namespace rtc2
