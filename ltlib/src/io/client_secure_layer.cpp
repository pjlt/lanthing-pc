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

#include "client_secure_layer.h"
#include "client_transport_layer.h"

#include <cstring>

#include <mbedtls/debug.h>
#include <mbedtls/error.h>

#include <ltlib/logging.h>
#include <ltlib/pragma_warning.h>

// 忘了在哪个开源项目抄了一部分，sorry。这种写法不是很好理解，有bug也不好修（刚修了一个，在tls_read之后没有重置input
// buffer，在遇到大消息bug就表现出来了） 一直想重写，但是能run，就没动手??

namespace {

void tls_debug_log(void* ctx, int level, const char* file, int line, const char* str) {
    (void)ctx;
    LOGF(DEBUG, "tlslog: [%d] [%s:%d] %s", level, file, line, str);
}

} // namespace

namespace ltlib {

constexpr uint32_t TLS_BUF_SZ = 32 * 1024;

enum TLS_RESULT {
    TLS_OK = 0,
    TLS_ERR = -1,
    TLS_EOF = -2,
    TLS_READ_AGAIN = -3,
    TLS_MORE_AVAILABLE = -4,
    TLS_HAS_WRITE = -5,
};

struct msg {
    size_t len;
    uint8_t* buf;

    STAILQ_ENTRY(msg)
    next;
};

bool is_handshake_continue(int state) {
    return state != MBEDTLS_SSL_HANDSHAKE_OVER && state != MBEDTLS_SSL_HELLO_REQUEST;
}

MbedtlsCTransport::MbedtlsCTransport(const Params& params)
    : uvtransport_{make_uv_params(params)}
    , on_connected_{params.on_connected}
    , on_closed_{params.on_closed}
    , on_reconnecting_{params.on_reconnecting}
    , on_read_{params.on_read}
    , more_buffer_(TLS_BUF_SZ)
    , cert_content_{params.cert} {}

MbedtlsCTransport::~MbedtlsCTransport() {
    mbedtls_ctr_drbg_free(&drbg_);
}

bool MbedtlsCTransport::init() {
    if (!tls_init_context()) {
        return false;
    }
    if (!tls_init_engine()) {
        return false;
    }
    if (!uvtransport_.init()) {
        return false;
    }
    return true;
}

bool MbedtlsCTransport::tls_init_context() {
    mbedtls_ssl_config_init(&ssl_cfg_);
    mbedtls_ssl_conf_dbg(&ssl_cfg_, tls_debug_log, nullptr);
    mbedtls_debug_set_threshold(0);
    mbedtls_ssl_config_defaults(&ssl_cfg_, MBEDTLS_SSL_IS_CLIENT, MBEDTLS_SSL_TRANSPORT_STREAM,
                                MBEDTLS_SSL_PRESET_DEFAULT);
    mbedtls_ssl_conf_renegotiation(&ssl_cfg_, MBEDTLS_SSL_RENEGOTIATION_ENABLED);
    mbedtls_ssl_conf_authmode(&ssl_cfg_, MBEDTLS_SSL_VERIFY_REQUIRED);
    ::memset(&entropy_, 0, sizeof(entropy_));
    mbedtls_entropy_init(&entropy_);
    mbedtls_ctr_drbg_init(&drbg_);
    std::unique_ptr<uint8_t[]> seed{new uint8_t[MBEDTLS_ENTROPY_MAX_SEED_SIZE]};
    mbedtls_ctr_drbg_seed(&drbg_, mbedtls_entropy_func, &entropy_, seed.get(),
                          MBEDTLS_ENTROPY_MAX_SEED_SIZE);
    mbedtls_ssl_conf_rng(&ssl_cfg_, mbedtls_ctr_drbg_random, &drbg_);
    mbedtls_x509_crt_init(&own_cert_);
    int ret = mbedtls_x509_crt_parse(&own_cert_,
                                     reinterpret_cast<const unsigned char*>(cert_content_.c_str()),
                                     cert_content_.size() + 1);
    if (ret != 0) {
        LOG(ERR) << "Parse cert file failed: " << ret;
        return false;
    }
    mbedtls_ssl_conf_ca_chain(&ssl_cfg_, &own_cert_, nullptr);
    return true;
}

bool MbedtlsCTransport::tls_init_engine() {
    mbedtls_ssl_init(&ssl_);
    mbedtls_ssl_setup(&ssl_, &ssl_cfg_);
    const std::string& hostname =
        uvtransport_.is_tcp() ? uvtransport_.host() : uvtransport_.pipe_name();
    // std::string hostname = "lanthing.net";
    mbedtls_ssl_set_hostname(&ssl_, hostname.c_str());
    // session_ = std::make_unique<mbedtls_ssl_session>();
    // memset(session_.get(), 0, sizeof(mbedtls_ssl_session));
    bio_in_ = BIO::create();
    bio_out_ = BIO::create();
    mbedtls_ssl_set_bio(&ssl_, this, mbed_ssl_send, mbed_ssl_recv, nullptr);
    return true;
}

int MbedtlsCTransport::tls_reset_engine() {
    // if (mbedtls_ssl_get_session(&ssl_, session_.get()) != 0) {
    //     mbedtls_ssl_session_free(session_.get());
    // }
    if (bio_in_) {
        BIO::destroy(bio_in_);
        bio_in_ = BIO::create();
    }
    if (bio_out_) {
        BIO::destroy(bio_out_);
        bio_out_ = BIO::create();
    }
    return mbedtls_ssl_session_reset(&ssl_);
}

int MbedtlsCTransport::tls_write(const char* data, uint32_t data_len, char* out,
                                 decltype(Buffer::len)* out_bytes, uint32_t maxout) {
    size_t wrote = 0;
    while (data_len > wrote) {
        int rc = mbedtls_ssl_write(&ssl_, (const unsigned char*)(data + wrote), data_len - wrote);
        if (rc < 0) {
            error_ = rc;
            return rc;
        }
        wrote += rc;
    }
    *out_bytes = bio_out_->read(reinterpret_cast<uint8_t*>(out), maxout);
    return static_cast<int>(bio_out_->available);
}

int MbedtlsCTransport::tls_read(const char* ssl_in, uint32_t ssl_in_len, char* out,
                                decltype(Buffer::len)* out_bytes, uint32_t maxout) {
    if (ssl_in_len > 0 && ssl_in != NULL) {
        bio_in_->put(reinterpret_cast<const uint8_t*>(ssl_in), ssl_in_len);
    }

    int rc;
    uint8_t* writep = (uint8_t*)out;
    size_t total_out = 0;

    do {
        rc = mbedtls_ssl_read(&ssl_, writep, maxout - total_out);

        if (rc > 0) {
            total_out += rc;
            writep += rc;
        }
    } while (rc > 0 && (maxout - total_out) > 0);

    *out_bytes = static_cast<uint32_t>(total_out);

    // this indicates that more bytes are needed to complete SSL frame
    if (rc == MBEDTLS_ERR_SSL_WANT_READ) {
        return bio_out_->available > 0 ? TLS_HAS_WRITE : TLS_OK;
    }

    if (rc == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) {
        return TLS_EOF;
    }

    if (rc < 0) {
        error_ = rc;
        char err[1024];
        mbedtls_strerror(rc, err, 1024);
        LOGF(ERR, "TLS error: %0x(%s)", rc, err);
        return TLS_ERR;
    }

    if (bio_in_->available > 0 || mbedtls_ssl_check_pending(&ssl_)) {
        return TLS_MORE_AVAILABLE;
    }

    return TLS_OK;
}

CTransport::Params MbedtlsCTransport::make_uv_params(const Params& params) {
    Params uvparams = params;
    uvparams.on_connected = std::bind(&MbedtlsCTransport::on_uv_connected, this);
    uvparams.on_closed = std::bind(&MbedtlsCTransport::on_uv_closed, this);
    uvparams.on_reconnecting = std::bind(&MbedtlsCTransport::on_uv_reconnecting, this);
    uvparams.on_read = std::bind(&MbedtlsCTransport::on_uv_read, this, std::placeholders::_1);
    return uvparams;
}

bool MbedtlsCTransport::on_uv_read(const Buffer& uvbuf) {
    int state = ssl_.MBEDTLS_PRIVATE(state);
    if (is_handshake_continue(state)) {
        Buffer buff{TLS_BUF_SZ};
        auto hs_state = continue_handshake(uvbuf.base, uvbuf.len, buff.base, &buff.len, TLS_BUF_SZ);
        if (buff.len > 0) {
            int success = uvtransport_.send(&buff, 1, [buff]() { delete buff.base; });
            if (!success) {
                delete buff.base;
                return false;
            }
        }
        else {
            delete buff.base;
        }
        if (hs_state == HandshakeState::COMPLETE) {
            return on_connected_();
        }
        else if (hs_state == HandshakeState::ERROR_) {
            char errbuf[1024];
            mbedtls_strerror(error_, errbuf, sizeof(errbuf));
            LOG(ERR) << "TLS handshake error:" << errbuf;
            return false;
        }
    }
    else if (state == MBEDTLS_SSL_HANDSHAKE_OVER) {
        Buffer outbuff = uvbuf;
        Buffer inbuff = uvbuf;
        enum TLS_RESULT rc = TLS_MORE_AVAILABLE;
        while (rc == TLS_MORE_AVAILABLE || rc == TLS_READ_AGAIN) {
            // uint32_t out_bytes = 0;
            rc = (TLS_RESULT)tls_read(inbuff.base, inbuff.len, outbuff.base, &outbuff.len,
                                      outbuff.len);
            inbuff.base = nullptr;
            inbuff.len = 0;
            switch (rc) {
            case ltlib::TLS_OK:
                on_read_(outbuff);
                break;
            case ltlib::TLS_EOF:
                if (outbuff.len > 0) {
                    on_read_(outbuff);
                }
                return false;
            case ltlib::TLS_READ_AGAIN:
            case ltlib::TLS_MORE_AVAILABLE:
                on_read_(outbuff);
                outbuff.base = more_buffer_.data();
                outbuff.len = static_cast<uint32_t>(more_buffer_.size());
                break;
            case ltlib::TLS_HAS_WRITE:
            {
                Buffer writebuf{TLS_BUF_SZ};
                int tls_rc = tls_write(nullptr, 0, writebuf.base, &writebuf.len, TLS_BUF_SZ);
                (void)tls_rc; // FIXME: tls_rc
                bool success =
                    uvtransport_.send(&writebuf, 1, [writebuf]() { delete writebuf.base; });
                if (!success) {
                    return false;
                }
                break;
            }
            case ltlib::TLS_ERR:
            default:
                if (outbuff.len > 0) {
                    on_read_(outbuff);
                }
                if (rc == ltlib::TLS_ERR) {
                    LOG(ERR) << "Unexpected TLS result:" << rc;
                }
                else {
                    char errbuf[1024];
                    mbedtls_strerror(error_, errbuf, 1024);
                    LOG(ERR) << "TLS error:" << errbuf;
                }
                return false;
            }
        }
    }
    return true;
}

void MbedtlsCTransport::on_uv_closed() {
    int ret = tls_reset_engine();
    if (ret != 0) {
        LOG(ERR) << "Reset ssl session failed:" << ret;
    }
    on_closed_();
}

void MbedtlsCTransport::on_uv_reconnecting() {
    int ret = tls_reset_engine();
    if (ret != 0) {
        LOG(ERR) << "Reset ssl session failed:" << ret;
    }
    on_reconnecting_();
}

bool MbedtlsCTransport::on_uv_connected() {
    // tls_reset_engine();
    int state = ssl_.MBEDTLS_PRIVATE(state);
    LOG(DEBUG) << "Start tls handshake " << state;
    if (is_handshake_continue(state)) {
        LOGF(ERR, "Start hanshake in the middle of another handshak(%d)", state);
        return false;
    }
    Buffer buff{TLS_BUF_SZ};
    continue_handshake(nullptr, 0, buff.base, &buff.len, TLS_BUF_SZ);
    int success = uvtransport_.send(&buff, 1, [buff]() { delete buff.base; });
    if (!success) {
        delete buff.base;
        return false;
    }
    return true;
}

MbedtlsCTransport::HandshakeState
MbedtlsCTransport::continue_handshake(char* in, uint32_t in_bytes, char* out,
                                      decltype(Buffer::len)* out_bytes, uint32_t maxout) {
    if (in_bytes > 0) {
        bio_in_->put(reinterpret_cast<const uint8_t*>(in), in_bytes);
    }
    // if (ssl_.MBEDTLS_PRIVATE(state) == MBEDTLS_SSL_HELLO_REQUEST && session_ != nullptr) {
    //     mbedtls_ssl_set_session(&ssl_, session_.get());
    //     mbedtls_ssl_session_free(session_.get());
    // }
    int state = mbedtls_ssl_handshake(&ssl_);
    char err[1024];
    mbedtls_strerror(state, err, 1024);
    *out_bytes = bio_out_->read(reinterpret_cast<uint8_t*>(out), maxout);

    if (ssl_.MBEDTLS_PRIVATE(state) == MBEDTLS_SSL_HANDSHAKE_OVER) {
        return HandshakeState::COMPLETE;
    }
    else if (state == MBEDTLS_ERR_SSL_WANT_READ || state == MBEDTLS_ERR_SSL_WANT_WRITE) {
        return HandshakeState::CONTINUE;
    }
    else {
        error_ = state;
        return HandshakeState::ERROR_;
    }
}

int MbedtlsCTransport::mbed_ssl_send(void* ctx, const uint8_t* buf, size_t len) {
    auto that = reinterpret_cast<MbedtlsCTransport*>(ctx);
    that->bio_out_->put(buf, len);
    return static_cast<int>(len);
}

int MbedtlsCTransport::mbed_ssl_recv(void* ctx, uint8_t* buf, size_t len) {
    auto that = reinterpret_cast<MbedtlsCTransport*>(ctx);
    if (that->bio_in_->available == 0) {
        return MBEDTLS_ERR_SSL_WANT_READ;
    }
    return that->bio_in_->read(buf, len);
}

bool MbedtlsCTransport::send(Buffer buff[], uint32_t buff_count,
                             const std::function<void()>& callback) {
    int tls_rc = 0;
    uint32_t out_size;
    for (uint32_t i = 0; i < buff_count; i++) {
        tls_rc = tls_write(buff[i].base, buff[i].len, nullptr, &out_size, 0);
        if (tls_rc < 0) {
            char errbuf[1024] = {0};
            mbedtls_strerror(error_, errbuf, 1024);
            LOG(ERR) << "tls_write failed:" << errbuf;
            return false;
        }
    }
    if (tls_rc > 0) {
        Buffer outbuf{uint32_t(tls_rc)};
        tls_rc = tls_write(nullptr, 0, outbuf.base, &outbuf.len, tls_rc);
        if (tls_rc < 0) {
            char errbuf[1024] = {0};
            mbedtls_strerror(error_, errbuf, 1024);
            LOG(ERR) << "tls_write failed:" << errbuf;
            return false;
        }
        else {
            bool success = uvtransport_.send(&outbuf, 1, [outbuf, callback]() {
                delete outbuf.base;
                if (callback) {
                    callback();
                }
            });
            return success;
        }
    }
    else if (tls_rc == 0) {
        return true;
    }

    return true;
}

void MbedtlsCTransport::reconnect() {
    uvtransport_.reconnect();
}

WARNING_DISABLE(6011)
WARNING_DISABLE(6001)
BIO* BIO::create() {

    BIO* bio = (BIO*)calloc(1, sizeof(BIO));

    bio->headoffset = 0;
    bio->qlen = 0;

    STAILQ_INIT(&bio->message_q);
    return bio;
}

void BIO::destroy(BIO* b) {

    while (!STAILQ_EMPTY(&b->message_q)) {
        struct msg* m = STAILQ_FIRST(&b->message_q);
        STAILQ_REMOVE_HEAD(&b->message_q, next);
        free(m->buf);
        free(m);
    }

    free(b);
}

int BIO::put(const uint8_t* buf, size_t len) {
    struct msg* m = (struct msg*)malloc(sizeof(struct msg));
    if (m == NULL) {
        return -1;
    }

    m->buf = (uint8_t*)malloc(len);
    if (m->buf == NULL) {
        free(m);
        return -1;
    }
    memcpy(m->buf, buf, len);

    m->len = len;

    STAILQ_INSERT_TAIL(&message_q, m, next);
    available += len;
    qlen += 1;

    return static_cast<int>(len);
}

int BIO::read(uint8_t* buf, size_t len) {
    size_t total = 0;

    while (!STAILQ_EMPTY(&message_q) && total < len) {
        struct msg* m = STAILQ_FIRST(&message_q);

        size_t recv_size = std::min(len - total, m->len - headoffset);
        memcpy(buf + total, m->buf + headoffset, recv_size);
        headoffset += recv_size;
        available -= recv_size;
        total += recv_size;

        if (headoffset == m->len) {
            STAILQ_REMOVE_HEAD(&message_q, next);
            headoffset = 0;
            qlen -= 1;

            free(m->buf);
            free(m);
        }
    }

    return (int)total;
}

WARNING_ENABLE(6011)
WARNING_ENABLE(6001)

} // namespace ltlib