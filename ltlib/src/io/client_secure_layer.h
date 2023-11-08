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
#include "client_transport_layer.h"
#include "queue.h"
#include <cstdint>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ssl.h>
#include <vector>

namespace ltlib {

// TODO: REPLACE BIO
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

class MbedtlsCTransport : public CTransport {
private:
    enum class HandshakeState { BEFORE, CONTINUE, COMPLETE, ERROR_ };

public:
    MbedtlsCTransport(const Params& params);
    ~MbedtlsCTransport() override;
    bool init() override;
    bool send(Buffer buff[], uint32_t buff_count, const std::function<void()>& callback) override;
    void reconnect() override;

private:
    bool tls_init_context();
    bool tls_init_engine();
    int tls_reset_engine();
    int tls_write(const char* data, uint32_t data_len, char* out, decltype(Buffer::len)* out_bytes,
                  uint32_t maxout);
    int tls_read(const char* ssl_in, uint32_t ssl_in_len, char* out,
                 decltype(Buffer::len)* out_bytes, uint32_t maxout);
    Params make_uv_params(const Params& params);
    bool on_uv_read(const Buffer&);
    void on_uv_closed();
    void on_uv_reconnecting();
    bool on_uv_connected();

    HandshakeState continue_handshake(char* in, uint32_t in_bytes, char* out,
                                      decltype(Buffer::len)* out_bytes, uint32_t maxout);
    static int mbed_ssl_send(void* ctx, const uint8_t* buf, size_t len);
    static int mbed_ssl_recv(void* ctx, uint8_t* buf, size_t len);

private:
    LibuvCTransport uvtransport_;
    // 下面几个是所有connections共有的，但我们这里只有1个connection
    mbedtls_ssl_config ssl_cfg_;
    mbedtls_pk_context own_key_;
    mbedtls_x509_crt own_cert_;
    // 下面几个是属于某一个connection
    mbedtls_ssl_context ssl_;
    // std::unique_ptr<mbedtls_ssl_session> session_;
    mbedtls_ctr_drbg_context drbg_;
    mbedtls_entropy_context entropy_;
    BIO* bio_in_;
    BIO* bio_out_;
    int error_;

    std::function<bool()> on_connected_;
    std::function<void()> on_closed_;
    std::function<void()> on_reconnecting_;
    std::function<bool(const Buffer&)> on_read_;
    std::vector<char> more_buffer_;
    const std::string cert_content_;
};

} // namespace ltlib