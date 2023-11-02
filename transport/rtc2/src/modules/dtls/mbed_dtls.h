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

#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ssl.h>
#include <mbedtls/timing.h>

#include <rtc2/key_and_cert.h>

#include <modules/dtls/queue.h>

namespace rtc2 {

// TODO: 去掉BIO，DTLS不需要这个玩意，有了反而负优化
// TODO: 支持SRTP
class MbedDtls {

    enum class HandshakeState { BEFORE, CONTINUE, COMPLETE, ERROR_ };

    struct msg {
        size_t len;
        uint8_t* buf;

        STAILQ_ENTRY(msg)
        next;
    };

    struct BIO {
        static BIO* create();
        static void destroy(BIO* b);
        int put(const uint8_t* buf, size_t len);
        int read(uint8_t* buf, size_t len);

        size_t available;
        size_t headoffset;
        unsigned int qlen;
        STAILQ_HEAD(msgq, msg)
        message_q;
    };

public:
    struct Params {
        std::function<void(const uint8_t* data, uint32_t size)> write_to_network;
        std::function<void(const uint8_t* data, uint32_t size)> on_receive;
        std::function<void(bool success)> on_handshake_done;
        std::function<void()> on_eof;
        std::function<void()> on_tls_error;
        std::shared_ptr<KeyAndCert> key_and_cert;
        bool is_server;
        std::vector<uint8_t> peer_digest;
    };

public:
    static std::unique_ptr<MbedDtls> create(const Params& params);
    ~MbedDtls();
    bool startHandshake();
    bool onNetworkData(const uint8_t* data, uint32_t size);
    bool send(const uint8_t* data, uint32_t size);

private:
    MbedDtls(const Params& params);
    bool init();
    bool tls_init_context();
    bool tls_init_engine();
    // 将业务数据写入SSL
    int write_app_to_ssl(const uint8_t* data, uint32_t data_len);
    // 将解密后的业务数据从SSL中读出来
    int read_app_from_ssl();
    HandshakeState continue_handshake();
    static int ssl_send_cb(void* ctx, const uint8_t* buf, size_t len);
    static int ssl_recv_cb(void* ctx, uint8_t* buf, size_t len);
    static int verify_cert(void* data, mbedtls_x509_crt* crt, int depth, uint32_t* flags);

private:
    std::function<void(const uint8_t* data, uint32_t size)> write_to_network_;
    std::function<void(const uint8_t* data, uint32_t size)> on_receive_;
    std::function<void(bool success)> on_handshake_done_;
    std::function<void()> on_eof_;
    std::function<void()> on_tls_error_;
    std::shared_ptr<KeyAndCert> key_cert_;
    bool is_server_;
    std::vector<uint8_t> peer_digest_;
    std::vector<uint8_t> buffer_;

    mbedtls_ssl_config ssl_cfg_;
    mbedtls_ssl_context ssl_;
    mbedtls_ctr_drbg_context drbg_;
    mbedtls_entropy_context entropy_;
    mbedtls_timing_delay_context timer_{};
    BIO* bio_in_ = nullptr;
    std::vector<int> ciphersuites_;
};

} // namespace rtc2