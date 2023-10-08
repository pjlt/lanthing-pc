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

#include <rtc2/key_and_cert.h>

#include <chrono>

#include <ltlib/logging.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/pk.h>
#include <mbedtls/x509_crt.h>

namespace rtc2 {

class KeyAndCertImpl {
public:
    KeyAndCertImpl();
    ~KeyAndCertImpl();

    KeyAndCertImpl(const KeyAndCertImpl&) = delete;
    KeyAndCertImpl& operator=(const KeyAndCertImpl&) = delete;
    KeyAndCertImpl(KeyAndCertImpl&&) = delete;
    KeyAndCertImpl& operator=(KeyAndCertImpl&&) = delete;

    bool createInternal();

    mbedtls_pk_context* key();
    mbedtls_x509_crt* cert();
    const std::vector<uint8_t>& digest() const;

private:
    bool calcDigest();
    static tm localtime(const std::time_t& ts);

private:
    mbedtls_pk_context key_;
    mbedtls_x509_crt cert_;
    std::vector<uint8_t> digest_;
};

KeyAndCertImpl::KeyAndCertImpl() {
    mbedtls_pk_init(&key_);
    mbedtls_x509_crt_init(&cert_);
}

KeyAndCertImpl::~KeyAndCertImpl() {
    mbedtls_pk_free(&key_);
    mbedtls_x509_crt_free(&cert_);
}

bool KeyAndCertImpl::createInternal() {
    std::shared_ptr<mbedtls_entropy_context> entropy(new mbedtls_entropy_context,
                                                     [](mbedtls_entropy_context* p) {
                                                         mbedtls_entropy_free(p);
                                                         delete p;
                                                     });
    std::shared_ptr<mbedtls_ctr_drbg_context> drbg(new mbedtls_ctr_drbg_context,
                                                   [](mbedtls_ctr_drbg_context* p) {
                                                       mbedtls_ctr_drbg_free(p);
                                                       delete p;
                                                   });
    mbedtls_entropy_init(entropy.get());
    mbedtls_ctr_drbg_init(drbg.get());
    std::unique_ptr<uint8_t[]> seed{new uint8_t[128]};
    int ret =
        mbedtls_ctr_drbg_seed(drbg.get(), mbedtls_entropy_func, entropy.get(), seed.get(), 128);
    if (ret != 0) {
        LOG(ERR) << "mbedtls_ctr_drbg_seed failed " << ret;
        return false;
    }
    ret = mbedtls_pk_setup(&key_, mbedtls_pk_info_from_type(MBEDTLS_PK_RSA));
    if (ret != 0) {
        LOG(ERR) << "mbedtls_pk_setup failed: " << ret;
        return false;
    }
    ret =
        mbedtls_rsa_gen_key(mbedtls_pk_rsa(key_), mbedtls_ctr_drbg_random, drbg.get(), 2048, 65537);
    if (ret != 0) {
        LOG(ERR) << "mbedtls_rsa_gen_key failed: " << ret;
        return false;
    }
    std::shared_ptr<mbedtls_x509write_cert> write_cert(new mbedtls_x509write_cert,
                                                       [](mbedtls_x509write_cert* cert) {
                                                           mbedtls_x509write_crt_free(cert);
                                                           delete cert;
                                                       });
    mbedtls_x509write_crt_init(write_cert.get());
    mbedtls_x509write_crt_set_subject_key(write_cert.get(), &key_);
    mbedtls_x509write_crt_set_issuer_key(write_cert.get(), &key_);
    if (mbedtls_x509write_crt_set_subject_name(write_cert.get(), "CN=Lanthing") != 0 ||
        mbedtls_x509write_crt_set_issuer_name(write_cert.get(), "CN=Numbaa") != 0) {
        LOG(ERR) << "mbedtls_x509write_crt_set name failed";
        return false;
    }
    mbedtls_x509write_crt_set_version(write_cert.get(), MBEDTLS_X509_CRT_VERSION_3);
    mbedtls_x509write_crt_set_md_alg(write_cert.get(), MBEDTLS_MD_SHA256);
    std::shared_ptr<mbedtls_mpi> serial(new mbedtls_mpi, [](mbedtls_mpi* mpi) {
        mbedtls_mpi_free(mpi);
        delete mpi;
    });
    mbedtls_mpi_init(serial.get());
    mbedtls_mpi_fill_random(serial.get(), 8, mbedtls_ctr_drbg_random, drbg.get());
    ret = mbedtls_x509write_crt_set_serial(write_cert.get(), serial.get());
    if (ret != 0) {
        LOG(ERR) << "mbedtls_x509write_crt_set_serial failed: " << ret;
        return false;
    }
    auto now = std::chrono::system_clock::now();
    auto kOneDay = std::chrono::days{1};
    auto k30Days = std::chrono::days{30};
    auto not_before = localtime(std::chrono::system_clock::to_time_t(now - kOneDay));
    auto not_after = localtime(std::chrono::system_clock::to_time_t(now + k30Days));
    // 我的not_before，not_after是utc时区还是当前时区？
    std::vector<char> not_before_buffer(20, 0);
    std::vector<char> not_after_buffer(20, 0);
    strftime(not_before_buffer.data(), not_before_buffer.size(), "%Y%m%d%H%M%S", &not_before);
    strftime(not_after_buffer.data(), not_after_buffer.size(), "%Y%m%d%H%M%S", &not_after);
    ret = mbedtls_x509write_crt_set_validity(write_cert.get(), not_before_buffer.data(),
                                             not_after_buffer.data());
    if (ret != 0) {
        LOG(ERR) << "mbedtls_x509write_crt_set_validity failed: " << ret;
        return false;
    }
    std::vector<unsigned char> buffer(4096, 0);
    ret = mbedtls_x509write_crt_der(write_cert.get(), buffer.data(), buffer.size(),
                                    mbedtls_ctr_drbg_random, drbg.get());
    if (ret <= 0) {
        LOG(ERR) << "mbedtls_x509write_crt_der failed: " << ret;
        return false;
    }
    ret = mbedtls_x509_crt_parse(&cert_, buffer.data() + buffer.size() - ret, ret);
    if (ret != 0) {
        LOG(ERR) << "mbedtls_x509_crt_parse failed: " << ret;
        return false;
    }
    if (!calcDigest()) {
        return false;
    }
    return true;
}

mbedtls_pk_context* KeyAndCertImpl::key() {
    return &key_;
}

mbedtls_x509_crt* KeyAndCertImpl::cert() {
    return &cert_;
}

const std::vector<uint8_t>& KeyAndCertImpl::digest() const {
    return digest_;
}

bool KeyAndCertImpl::calcDigest() {
    std::shared_ptr<mbedtls_md_context_t> md_ctx(new mbedtls_md_context_t,
                                                 [](mbedtls_md_context_t* ctx) {
                                                     mbedtls_md_free(ctx);
                                                     delete ctx;
                                                 });
    mbedtls_md_init(md_ctx.get());
    auto info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    if (info == nullptr) {
        LOG(ERR) << "mbedtls_md_info_from_type(MBEDTLS_MD_SHA256) failed";
        return false;
    }
    int ret = mbedtls_md_setup(md_ctx.get(), info, 0);
    if (ret != 0) {
        LOG(ERR) << "mbedtls_md_setup failed";
        return false;
    }
    std::vector<uint8_t> sha256(32);
    if (mbedtls_md_starts(md_ctx.get()) != 0 ||
        mbedtls_md_update(md_ctx.get(), cert_.raw.p, cert_.raw.len) != 0 ||
        mbedtls_md_finish(md_ctx.get(), sha256.data())) {
        LOG(ERR) << "Calculate sha256 for cerificate failed";
        return false;
    }
    digest_ = sha256;
    return true;
}

tm KeyAndCertImpl::localtime(const std::time_t& ts) {
    struct tm tm_snapshot;
#if (defined(WIN32) || defined(_WIN32) || defined(__WIN32__))
    localtime_s(&tm_snapshot, &ts); // windsows
#else
    localtime_r(&ts, &tm_snapshot); // POSIX
#endif
    return tm_snapshot;
}

std::shared_ptr<KeyAndCert> KeyAndCert::create() {
    auto impl = std::make_shared<KeyAndCertImpl>();
    if (!impl->createInternal()) {
        return nullptr;
    }
    auto kns = std::make_shared<KeyAndCert>();
    kns->impl_ = impl;
    return kns;
}

mbedtls_pk_context* KeyAndCert::key() {
    return impl_->key();
}

mbedtls_x509_crt* KeyAndCert::cert() {
    return impl_->cert();
}

const std::vector<uint8_t>& KeyAndCert::digest() const {
    return impl_->digest();
}

} // namespace rtc2