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

#include "udp_socket.h"

#include <ltlib/logging.h>
#include <uv.h>

#include <ltlib/times.h>

namespace rtc2 {

class UDPSocketImpl {
public:
    static std::shared_ptr<UDPSocketImpl> create(ltlib::IOLoop* ioloop, const Address& bind_addr);
    ~UDPSocketImpl();
    int32_t sendmsg(std::vector<std::span<const uint8_t>> spans, const Address& addr);
    int32_t error();
    void setOnRead(const UDPSocket::OnRead& on_read);
    uint16_t port();
    static void on_alloc_memory(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf);
    static void on_udp_recv(uv_udp_t* handle, ssize_t nread, const uv_buf_t* buf,
                            const struct sockaddr* addr, unsigned flags);
    static void on_udp_sent(uv_udp_send_t* req, int status);

private:
    uv_udp_t* udp_ = nullptr;
    Address bind_addr_{};
    int32_t error_ = 0;
    UDPSocket::OnRead on_read_ = nullptr;
};

std::shared_ptr<UDPSocketImpl> UDPSocketImpl::create(ltlib::IOLoop* ioloop,
                                                     const Address& bind_addr) {
    auto udp = new uv_udp_t;
    memset(udp, 0, sizeof(uv_udp_t));
    auto uvloop = reinterpret_cast<uv_loop_t*>(ioloop->context());
    int ret = uv_udp_init_ex(uvloop, udp, AF_INET);
    if (ret != 0) {
        delete udp;
        LOG(ERR) << "uv_udp_init_ex failed with " << ret;
        return nullptr;
    }
    auto storage = bind_addr.to_storage();
    ret = uv_udp_bind(udp, reinterpret_cast<const sockaddr*>(&storage), UV_UDP_REUSEADDR);
    if (ret != 0) {
        uv_close((uv_handle_t*)udp, [](uv_handle_t* handle) {
            auto udp = (uv_udp_t*)handle;
            delete udp;
        });
        LOG(ERR) << "uv_udp_bind failed with " << ret;
        return nullptr;
    }
    ret = uv_udp_recv_start(udp, UDPSocketImpl::on_alloc_memory, UDPSocketImpl::on_udp_recv);
    if (ret != 0) {
        uv_close((uv_handle_t*)udp, [](uv_handle_t* handle) {
            auto udp = (uv_udp_t*)handle;
            delete udp;
        });
        LOG(ERR) << "uv_udp_recv_start failed with " << ret;
        return nullptr;
    }
    auto udp_socket = std::make_unique<UDPSocketImpl>();
    udp->data = udp_socket.get();
    udp_socket->udp_ = udp;
    udp_socket->bind_addr_ = bind_addr;
    return udp_socket;
}

UDPSocketImpl::~UDPSocketImpl() {
    if (udp_ == nullptr) {
        return;
    }
    uv_close((uv_handle_t*)udp_, [](uv_handle_t* handle) {
        auto udp = (uv_udp_t*)handle;
        delete udp;
    });
}

int32_t UDPSocketImpl::sendmsg(std::vector<std::span<const uint8_t>> spans, const Address& addr) {
    uv_udp_send_t* send_req = new uv_udp_send_t{};
    std::vector<uv_buf_t> buffs(spans.size());
    for (size_t i = 0; i < spans.size(); i++) {
        buffs[i].base = reinterpret_cast<char*>(const_cast<uint8_t*>(spans[i].data()));
        buffs[i].len = static_cast<decltype(uv_buf_t::len)>(spans[i].size());
    }
    send_req->data = this;
    auto storage = addr.to_storage();
    int ret = uv_udp_send(send_req, udp_, buffs.data(), static_cast<unsigned int>(spans.size()),
                          reinterpret_cast<const sockaddr*>(&storage), &UDPSocketImpl::on_udp_sent);
    if (ret != 0) {
        error_ = ret;
        delete send_req;
        return ret;
    }
    return 0;
}

int32_t UDPSocketImpl::error() {
    return error_;
}

void UDPSocketImpl::setOnRead(const UDPSocket::OnRead& on_read) {
    on_read_ = on_read;
}

uint16_t UDPSocketImpl::port() {
    // 所有socket都是bind过，都是我们设置的地址，不需要再调api取地址
    return bind_addr_.port();
}

void UDPSocketImpl::on_alloc_memory(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf) {
    (void)handle;
    buf->base = new char[suggested_size];
    buf->len = static_cast<decltype(buf->len)>(suggested_size);
}

void UDPSocketImpl::on_udp_recv(uv_udp_t* handle, ssize_t nread, const uv_buf_t* buf,
                                const sockaddr* addr, unsigned flags) {
    // 在我的使用场景里，这个flags似乎不用处理
    (void)flags;
    auto that = reinterpret_cast<UDPSocketImpl*>(handle->data);
    if (nread < 0) {
        that->error_ = static_cast<int32_t>(nread);
        delete buf->base;
        // TODO: 通知错误
        return;
    }
    if (that->on_read_ != nullptr) {
        // 整个过程没有复制，IPv4情况下应该不会爆炸吧？
        auto address = Address::from_storage(*reinterpret_cast<const sockaddr_storage*>(addr));
        that->on_read_(reinterpret_cast<const uint8_t*>(buf->base), static_cast<uint32_t>(nread),
                       address, ltlib::steady_now_us());
    }
    delete buf->base;
}

void UDPSocketImpl::on_udp_sent(uv_udp_send_t* req, int status) {
    auto that = reinterpret_cast<UDPSocketImpl*>(req->data);
    if (status < 0) {
        that->error_ = status;
    }
    else {
        // TODO: 通知错误
        that->error_ = 0;
    }
    delete req;
}

std::unique_ptr<UDPSocket> UDPSocket::create(ltlib::IOLoop* ioloop, const Address& bind_addr) {
    auto impl = UDPSocketImpl::create(ioloop, bind_addr);
    if (impl == nullptr) {
        return nullptr;
    }
    auto udp_socket = std::make_unique<UDPSocket>();
    udp_socket->impl_ = impl;
    return udp_socket;
}

int32_t UDPSocket::sendmsg(std::vector<std::span<const uint8_t>> spans, const Address& addr) {
    return impl_->sendmsg(spans, addr);
}

int32_t UDPSocket::error() {
    return impl_->error();
}

void UDPSocket::setOnRead(const OnRead& on_read) {
    impl_->setOnRead(on_read);
}

uint16_t UDPSocket::port() {
    return impl_->port();
}

} // namespace rtc2