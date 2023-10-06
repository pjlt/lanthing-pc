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

#pragma once
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <rtc2/key_and_cert.h>

#include <modules/dtls/mbed_dtls.h>
#include <modules/network/network_channel.h>

namespace rtc2 {

enum class DtlsState {
    New,
    Connecting,
    Connected,
    Closed,
    Failed,
};

class DtlsChannel {
public:
    struct Params {
        bool is_server;
        std::shared_ptr<KeyAndCert> key_and_cert;
        std::vector<uint8_t> remote_digest;
        NetworkChannel* network_channel;
        std::function<void(const uint8_t*, uint32_t, int64_t)> on_read_packet;
        std::function<void(const uint8_t*, uint32_t, int64_t)> on_read_rtp_packet;
        std::function<void()> on_connected;
        std::function<void()> on_disconnected;
    };

public:
    static std::unique_ptr<DtlsChannel> create(const Params& params);

    ~DtlsChannel();

    DtlsState dtls_state() const;

    int sendPacket(const uint8_t* data, uint32_t size, bool bypass);

private:
    DtlsChannel(const Params& params);
    bool init();
    // dtls
    void startHandshake();
    void onHandshakeDone(bool success);
    void writeToNetwork(const uint8_t* data, uint32_t size);
    void onDecryptedPacket(const uint8_t* data, uint32_t size);
    void onDtlsEOF();
    void onDtlsError();
    // network
    void onNetworkConnected(const EndpointInfo& local, const EndpointInfo& remote,
                            int64_t used_time_ms);
    void onReadNetPacket(const uint8_t* data, uint32_t size, int64_t time_us);

    bool checkAndHandleDtlsPacket(const uint8_t* data, uint32_t size);

private:
    NetworkChannel* network_channel_;
    std::shared_ptr<KeyAndCert> key_and_cert_;
    std::vector<uint8_t> remote_digest_;
    std::unique_ptr<MbedDtls> mbed_;
    DtlsState dtls_state_ = DtlsState::New;
    bool is_server_ = false;
    bool network_connected_ = false;
    std::vector<uint8_t> cached_client_hello_;
    std::function<void()> on_connected_;
    std::function<void()> on_disconnected_;
    std::function<void(const uint8_t*, uint32_t, int64_t)> on_read_packet_;
    std::function<void(const uint8_t*, uint32_t, int64_t)> on_read_rtp_packet_;
};

} // namespace rtc2