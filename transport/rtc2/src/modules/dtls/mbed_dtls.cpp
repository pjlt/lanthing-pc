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

#include "mbed_dtls.h"

#include <ltlib/logging.h>
#include <mbedtls/debug.h>
#include <mbedtls/error.h>

namespace {

enum TLS_RESULT {
    TLS_OK = 0,
    TLS_ERR = -1,
    TLS_EOF = -2,
    TLS_READ_AGAIN = -3,
    TLS_MORE_AVAILABLE = -4,
    TLS_HAS_WRITE = -5,
};

void tls_debug_log(void* ctx, int level, const char* file, int line, const char* str) {
    (void)ctx;
    LOGF(DEBUG, "tlslog: [%d] [%s:%d] %s", level, file, line, str);
}

bool is_handshake_continue(int state) {
    return state != MBEDTLS_SSL_HANDSHAKE_OVER && state != MBEDTLS_SSL_HELLO_REQUEST;
}

tm localtime(const std::time_t& ts) {
    struct tm tm_snapshot;
#if (defined(WIN32) || defined(_WIN32) || defined(__WIN32__))
    localtime_s(&tm_snapshot, &ts); // windsows
#else
    localtime_r(&ts, &tm_snapshot); // POSIX
#endif
    return tm_snapshot;
}

} // namespace

namespace rtc2 {

std::unique_ptr<MbedDtls> MbedDtls::create(const Params& params) {
    std::unique_ptr<MbedDtls> dtls{new MbedDtls(params)};
    if (!dtls->init()) {
        return nullptr;
    }
    return dtls;
}

MbedDtls::~MbedDtls() {
    mbedtls_ssl_config_free(&ssl_cfg_);
    mbedtls_ssl_free(&ssl_);
    mbedtls_entropy_free(&entropy_);
    mbedtls_ctr_drbg_free(&drbg_);
    if (bio_in_ != nullptr) {
        BIO::destroy(bio_in_);
    }
    if (bio_out_ != nullptr) {
        BIO::destroy(bio_out_);
    }
}

MbedDtls::MbedDtls(const Params& params)
    : write_to_network_{params.write_to_network}
    , on_receive_{params.on_receive}
    , on_handshake_done_{params.on_handshake_done}
    , on_eof_{params.on_eof}
    , on_tls_error_{params.on_tls_error}
    , key_cert_{params.key_and_cert}
    , is_server_{params.is_server}
    , peer_digest_{params.peer_digest} {
    buffer_.resize(32 * 1024);
    mbedtls_ssl_config_init(&ssl_cfg_);
    mbedtls_ssl_init(&ssl_);
    mbedtls_entropy_init(&entropy_);
    mbedtls_ctr_drbg_init(&drbg_);
}

bool MbedDtls::init() {
    if (!tls_init_context()) {
        return false;
    }
    if (tls_init_engine()) {
        return false;
    }
    return true;
}

bool MbedDtls::startHandshake() {
    uint32_t buffer_len = 0;
    continue_handshake(nullptr, 0, buffer_.data(), &buffer_len,
                       static_cast<uint32_t>(buffer_.size()));
    if (buffer_len != 0) {
        write_to_network_(buffer_.data(), buffer_len);
    }
    return true;
}

bool MbedDtls::onNetworkData(const uint8_t* data, uint32_t size) {
    int state = ssl_.MBEDTLS_PRIVATE(state);
    if (is_handshake_continue(state)) {
        uint32_t buff_len;
        auto hs_state = continue_handshake(data, size, buffer_.data(), &buff_len,
                                           static_cast<uint32_t>(buffer_.size()));
        if (buff_len > 0) {
            write_to_network_(buffer_.data(), buff_len);
        }
        if (hs_state == HandshakeState::COMPLETE) {
            on_handshake_done_(true);
        }
        else if (hs_state == HandshakeState::ERROR_) {
            on_handshake_done_(false);
            return false;
        }
    }
    else if (state == MBEDTLS_SSL_HANDSHAKE_OVER) {
        uint32_t outbuff_len = 0;
        enum TLS_RESULT rc = TLS_MORE_AVAILABLE;
        while (rc == TLS_MORE_AVAILABLE || rc == TLS_READ_AGAIN) {
            rc = (TLS_RESULT)tls_read(data, size, buffer_.data(), &outbuff_len,
                                      static_cast<uint32_t>(buffer_.size()));
            switch (rc) {
            case TLS_OK:
                if (outbuff_len > 0) {
                    on_receive_(buffer_.data(), outbuff_len);
                }
                break;
            case TLS_EOF:
                if (outbuff_len > 0) {
                    on_receive_(buffer_.data(), outbuff_len);
                }
                on_eof_();
                return false;
            case TLS_READ_AGAIN:
            case TLS_MORE_AVAILABLE:
                if (outbuff_len > 0) {
                    on_receive_(buffer_.data(), outbuff_len);
                }
                //??
                break;
            case TLS_HAS_WRITE:
            {
                std::vector<uint8_t> writebuf(32 * 1023);
                uint32_t writebuf_len = 0;
                int tls_rc = tls_write(nullptr, 0, writebuf.data(), &writebuf_len,
                                       static_cast<uint32_t>(writebuf.size()));
                (void)tls_rc; // FIXME: handle tls_rc，不过给了32KB的buffer，应该不用处理？
                if (writebuf_len > 0) {
                    write_to_network_(writebuf.data(), writebuf_len);
                }
                break;
            }
            case TLS_ERR:
            default:
                if (outbuff_len > 0) {
                    on_receive_(buffer_.data(), outbuff_len);
                }
                LOG(ERR) << "Unexpected TLS result:" << rc;
                on_tls_error_(); // 有必要？
                return false;
            }
        }
    }
    return true;
}

bool MbedDtls::send(const uint8_t* data, uint32_t size) {
    uint32_t out_size = 0;
    int tls_rc = tls_write(data, size, nullptr, &out_size, 0);
    if (tls_rc < 0) {
        return false;
    }
    else if (tls_rc > 0) {
        tls_rc = tls_write(nullptr, 0, buffer_.data(), &out_size, tls_rc);
        if (tls_rc < 0) {
            return false;
        }
        else {
            write_to_network_(buffer_.data(), out_size);
            return true;
        }
    }
    else {
        return true;
    }
}

bool MbedDtls::tls_init_context() {
    int endpoint = is_server_ ? MBEDTLS_SSL_IS_SERVER : MBEDTLS_SSL_IS_CLIENT;
    mbedtls_ssl_config_defaults(&ssl_cfg_, endpoint, MBEDTLS_SSL_TRANSPORT_DATAGRAM,
                                MBEDTLS_SSL_PRESET_DEFAULT);
    mbedtls_ssl_conf_dbg(&ssl_cfg_, tls_debug_log, nullptr);
    mbedtls_debug_set_threshold(0);
    // 服务端、客户端都是自己写的，写死一个就够了吧
    std::vector<std::string> ciphersuite_names = {
        "TLS-ECDHE-ECDSA-WITH-CHACHA20-POLY1305-SHA256",
        "TLS-ECDHE-RSA-WITH-CHACHA20-POLY1305-SHA256",
        "TLS-ECDHE-ECDSA-WITH-AES-128-GCM-SHA256",
        "TLS-ECDHE-RSA-WITH-AES-128-GCM-SHA256",
        "TLS-ECDHE-ECDSA-WITH-AES-128-CBC-SHA",
        "TLS-ECDHE-RSA-WITH-AES-128-CBC-SHA",
        "TLS-ECDHE-ECDSA-WITH-AES-256-CBC-SHA",
        "TLS-ECDHE-RSA-WITH-AES-256-CBC-SHA",
        "TLS-RSA-WITH-AES-128-GCM-SHA256",
        "TLS-RSA-WITH-AES-128-CBC-SHA",
        "TLS-RSA-WITH-AES-256-CBC-SHA",
    };
    for (auto& name : ciphersuite_names) {
        const mbedtls_ssl_ciphersuite_t* c = mbedtls_ssl_ciphersuite_from_string(name.c_str());
        if (c != nullptr) {
            ciphersuites_.push_back(c->MBEDTLS_PRIVATE(id));
            printf("Adding ciphersuite (%#x) %s\n", c->MBEDTLS_PRIVATE(id), name.c_str());
        }
    }
    if (ciphersuites_.empty()) {
        LOG(ERR) << "No ciphersuites";
        return false;
    }
    ciphersuites_.push_back(0);
    mbedtls_ssl_conf_ciphersuites(&ssl_cfg_, ciphersuites_.data());
    mbedtls_ssl_conf_authmode(&ssl_cfg_, MBEDTLS_SSL_VERIFY_REQUIRED);
    mbedtls_ssl_conf_min_version(&ssl_cfg_, MBEDTLS_SSL_MAJOR_VERSION_3,
                                 MBEDTLS_SSL_MINOR_VERSION_3);
    mbedtls_ssl_conf_verify(&ssl_cfg_, &verify_cert, this);
    std::unique_ptr<uint8_t[]> seed{new uint8_t[MBEDTLS_ENTROPY_MAX_SEED_SIZE]};
    int ret = mbedtls_ctr_drbg_seed(&drbg_, mbedtls_entropy_func, &entropy_, seed.get(),
                                    MBEDTLS_ENTROPY_MAX_SEED_SIZE);
    if (ret != 0) {
        LOG(ERR) << "mbedtls_ctr_drbg_seed failed " << ret;
        return false;
    }
    mbedtls_ssl_conf_rng(&ssl_cfg_, mbedtls_ctr_drbg_random, &drbg_);
    mbedtls_ssl_conf_read_timeout(&ssl_cfg_, 1000);

    mbedtls_ssl_conf_own_cert(&ssl_cfg_, key_cert_->cert(), key_cert_->key());

    if (is_server_) {
        // 这个场景不用管DoS，禁掉cookie没问题
        // mbedtls_ssl_cookie_init(&cookie_);
        // ret = mbedtls_ssl_cookie_setup(&cookie_, mbedtls_ctr_drbg_random, &drbg_);
        // if (ret != 0) {
        //	std::cout << "mbedtls_ssl_cookie_setup failed " << ret << std::endl;
        // }
        // mbedtls_ssl_conf_dtls_cookies(&ssl_cfg_, mbedtls_ssl_cookie_write,
        // mbedtls_ssl_cookie_check, 	&cookie_);
        mbedtls_ssl_conf_dtls_cookies(&ssl_cfg_, nullptr, nullptr, nullptr);
    }

    return true;
}

bool MbedDtls::tls_init_engine() {
    mbedtls_ssl_init(&ssl_);
    mbedtls_ssl_setup(&ssl_, &ssl_cfg_);
    mbedtls_ssl_set_mtu(&ssl_, 1400); // 可否中途换
    mbedtls_ssl_set_timer_cb(&ssl_, &timer_, mbedtls_timing_set_delay, mbedtls_timing_get_delay);
    bio_in_ = BIO::create();
    bio_out_ = BIO::create();
    mbedtls_ssl_set_bio(&ssl_, this, mbed_ssl_send, mbed_ssl_recv, nullptr);
    return true;
}

int MbedDtls::tls_write(const uint8_t* data, uint32_t data_len, uint8_t* out, uint32_t* out_bytes,
                        uint32_t maxout) {
    size_t wrote = 0;
    while (data_len > wrote) {
        int rc = mbedtls_ssl_write(&ssl_, (const unsigned char*)(data + wrote), data_len - wrote);
        if (rc < 0) {
            return rc;
        }
        wrote += rc;
    }
    *out_bytes = bio_out_->read(reinterpret_cast<uint8_t*>(out), maxout);
    return static_cast<int>(bio_out_->available);
}

int MbedDtls::tls_read(const uint8_t* ssl_in, uint32_t ssl_in_len, uint8_t* out,
                       uint32_t* out_bytes, uint32_t maxout) {
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

    if (rc == MBEDTLS_ERR_SSL_WANT_READ) {
        return bio_out_->available > 0 ? TLS_HAS_WRITE : TLS_OK;
    }

    if (rc == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) {
        return TLS_EOF;
    }

    if (rc < 0) {
        char err[1024];
        mbedtls_strerror(rc, err, 1024);
        LOGF(ERR, "TLS error: %#x(%s)", rc, err);
        return TLS_ERR;
    }

    if (bio_in_->available > 0 || mbedtls_ssl_check_pending(&ssl_)) {
        return TLS_MORE_AVAILABLE;
    }

    return TLS_OK;
}

MbedDtls::HandshakeState MbedDtls::continue_handshake(const uint8_t* in, uint32_t in_bytes,
                                                      uint8_t* out, uint32_t* out_bytes,
                                                      uint32_t maxout) {
    if (in_bytes > 0) {
        bio_in_->put(reinterpret_cast<const uint8_t*>(in), in_bytes);
    }
    // FIXME: 这里没有处理MBEDTLS_SSL_HELLO_REQUEST，会不会出问题？
    int state = mbedtls_ssl_handshake(&ssl_);
    *out_bytes = bio_out_->read(reinterpret_cast<uint8_t*>(out), maxout);

    if (ssl_.MBEDTLS_PRIVATE(state) == MBEDTLS_SSL_HANDSHAKE_OVER) {
        return HandshakeState::COMPLETE;
    }
    else if (state == MBEDTLS_ERR_SSL_WANT_READ || state == MBEDTLS_ERR_SSL_WANT_WRITE) {
        return HandshakeState::CONTINUE;
    }
    else {
        char err[1024];
        mbedtls_strerror(state, err, 1024);
        LOG(ERR) << "mbedtls_ssl_handshake: " << err;
        return HandshakeState::ERROR_;
    }
}

int MbedDtls::mbed_ssl_send(void* ctx, const uint8_t* buf, size_t len) {
    auto that = reinterpret_cast<MbedDtls*>(ctx);
    that->bio_out_->put(buf, len);
    return static_cast<int>(len);
}

int MbedDtls::mbed_ssl_recv(void* ctx, uint8_t* buf, size_t len) {
    auto that = reinterpret_cast<MbedDtls*>(ctx);
    if (that->bio_in_->available == 0) {
        return MBEDTLS_ERR_SSL_WANT_READ;
    }
    return that->bio_in_->read(buf, len);
}

int MbedDtls::verify_cert(void* data, mbedtls_x509_crt* crt, int depth, uint32_t* flags) {
    (void)depth;
    mbedtls_md_context_t md_ctx;
    mbedtls_md_init(&md_ctx);
    auto info = mbedtls_md_info_from_type(mbedtls_md_type_t::MBEDTLS_MD_SHA256);
    mbedtls_md_setup(&md_ctx, info, 0);
    mbedtls_md_starts(&md_ctx);
    std::vector<uint8_t> sha256(32);
    mbedtls_md_update(&md_ctx, crt->raw.p, crt->raw.len);
    mbedtls_md_finish(&md_ctx, sha256.data());
    mbedtls_md_free(&md_ctx);
    auto that = reinterpret_cast<MbedDtls*>(data);
    if (sha256 == that->peer_digest_) {
        *flags = 0;
        LOG(INFO) << "Valid certificate";
    }
    else {
        LOG(ERR) << "Invalid certificate";
    }
    return 0;
}

MbedDtls::BIO* MbedDtls::BIO::create() {
    BIO* bio = (BIO*)calloc(1, sizeof(BIO));
    bio->available = 0;
    bio->headoffset = 0;
    bio->qlen = 0;

    STAILQ_INIT(&bio->message_q);
    return bio;
}

void MbedDtls::BIO::destroy(BIO* b) {
    while (!STAILQ_EMPTY(&b->message_q)) {
        struct msg* m = STAILQ_FIRST(&b->message_q);
        STAILQ_REMOVE_HEAD(&b->message_q, next);
        free(m->buf);
        free(m);
    }

    free(b);
}

int MbedDtls::BIO::put(const uint8_t* buf, size_t len) {
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

int MbedDtls::BIO::read(uint8_t* buf, size_t len) {
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

} // namespace rtc2