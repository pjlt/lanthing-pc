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

#include "client_transport_layer.h"
#include <ltlib/logging.h>

namespace
{

struct UvWrittenInfo
{
    UvWrittenInfo(ltlib::LibuvCTransport* _that, const std::function<void()>& cb)
        : that(_that)
        , custom_callback(cb)
    {
    }
    ltlib::LibuvCTransport* that;
    std::function<void()> custom_callback;
};

class SimpleGuard
{
public:
    SimpleGuard(const std::function<void()>& cleanup)
        : cleanup_ { cleanup }
    {
    }
    ~SimpleGuard()
    {
        cleanup_();
    }

private:
    std::function<void()> cleanup_;
};

} // namespace

namespace ltlib
{

LibuvCTransport::LibuvCTransport(const Params& params)
    : stype_ { params.stype }
    , ioloop_ { params.ioloop }
    , pipe_name_ { params.pipe_name }
    , host_ { params.host }
    , port_ { params.port }
    , on_connected_ { params.on_connected }
    , on_closed_ { params.on_closed }
    , on_reconnecting_ { params.on_reconnecting }
    , on_read_ { params.on_read }
{
}

LibuvCTransport::~LibuvCTransport()
{
    if (stype_ == StreamType::TCP) {
        if (tcp_ == nullptr) {
            return;
        }
        auto tcp_handle = tcp_.release();
        if (ioloop_->isNotCurrentThread()) {
            ioloop_->post([tcp_handle]() {
                uv_close((uv_handle_t*)tcp_handle, [](uv_handle_t* handle) { delete (uv_tcp_t*)handle; });
            });
        } else {
            uv_close((uv_handle_t*)tcp_handle, [](uv_handle_t* handle) { delete (uv_tcp_t*)handle; });
        }

    } else {
        if (pipe_ == nullptr) {
            return;
        }
        auto pipe_conn = pipe_.release();
        if (ioloop_->isNotCurrentThread()) {
            ioloop_->post([pipe_conn]() {
                uv_close((uv_handle_t*)pipe_conn, [](uv_handle_t* handle) { delete (uv_pipe_t*)handle; });
            });
        } else {
            uv_close((uv_handle_t*)pipe_conn, [](uv_handle_t* handle) { delete (uv_pipe_t*)handle; });
        }
    }
}

bool LibuvCTransport::init()
{
    if (stype_ == StreamType::TCP) {
        return init_tcp();
    } else {
        return init_pipe();
    }
}

bool LibuvCTransport::init_tcp()
{
    auto resolve_req = new uv_getaddrinfo_t;
    memset(resolve_req, 0, sizeof(uv_getaddrinfo_t));
    resolve_req->data = this;
    int ret = uv_getaddrinfo(uvloop(), resolve_req, &LibuvCTransport::on_dns_resolve, host_.c_str(), nullptr, nullptr);
    if (ret != 0) {
        LOG(ERR) << "DNS query failed:" << ret;
        delete resolve_req;
        return false;
    }
    return true;
}

bool LibuvCTransport::init_pipe()
{
    pipe_ = std::make_unique<uv_pipe_t>();
    int ret = uv_pipe_init(uvloop(), pipe_.get(), 0);
    if (ret != 0) {
        LOG(ERR) << "Init pipe failed: " << ret;
        pipe_.reset(); // reset是为了告诉析构函数，不要close这个pipe
        return false;
    }
    pipe_->data = this;
    conn_req_ = std::make_unique<uv_connect_t>();
    conn_req_->data = this;
    uv_pipe_connect(conn_req_.get(), pipe_.get(), pipe_name_.c_str(), &LibuvCTransport::on_connected);
    return true;
}

uv_loop_t* LibuvCTransport::uvloop()
{
    return reinterpret_cast<uv_loop_t*>(ioloop_->context());
}

bool LibuvCTransport::send(Buffer buff[], uint32_t buff_count, const std::function<void()>& callback)
{
    if (!ioloop_->isCurrentThread()) {
        LOG(FATAL) << "Send data in wrong thread!";
        return false;
    }
    auto info = new UvWrittenInfo(this, callback);
    uv_write_t* write_req = new uv_write_t {};
    write_req->data = info;
    uv_buf_t* uvbuf = reinterpret_cast<uv_buf_t*>(buff);
    uv_stream_t* stream_handle = uvstream();
    int ret = uv_write(write_req, stream_handle, uvbuf, buff_count, &LibuvCTransport::on_written);
    if (ret != 0) {
        LOGF(ERR, "%s write failed:%d", is_tcp() ? "TCP" : "Pipe", ret);
        delete info;
        delete write_req;
        return false;
    }
    return true;
}

bool LibuvCTransport::is_tcp() const
{
    return stype_ == StreamType::TCP;
}

const std::string& LibuvCTransport::pipe_name()
{
    return pipe_name_;
}

const std::string& LibuvCTransport::host()
{
    return host_;
}

std::string LibuvCTransport::ip() const
{
    return local_ip_;
}

uint16_t LibuvCTransport::port() const
{
    return local_port_;
}

uv_stream_t* LibuvCTransport::uvstream()
{
    uv_stream_t* stream_handle = is_tcp() ? reinterpret_cast<uv_stream_t*>(tcp_.get()) : reinterpret_cast<uv_stream_t*>(pipe_.get());
    return stream_handle;
}

uv_handle_t* LibuvCTransport::uvhandle()
{
    uv_handle_t* handle = is_tcp() ? reinterpret_cast<uv_handle_t*>(tcp_.get()) : reinterpret_cast<uv_handle_t*>(pipe_.get());
    return handle;
}

uv_handle_t* LibuvCTransport::uvhandle_release()
{
    uv_handle_t* handle = is_tcp() ? reinterpret_cast<uv_handle_t*>(tcp_.release()) : reinterpret_cast<uv_handle_t*>(pipe_.release());
    return handle;
}

void LibuvCTransport::reconnect()
{
    uv_handle_t* conn = uvhandle_release();
    uv_close(conn, &LibuvCTransport::delay_reconnect);
    on_reconnecting_();
}

void LibuvCTransport::delay_reconnect(uv_handle_t* handle)
{
    auto that = (LibuvCTransport*)handle->data;
    if (that->is_tcp()) {
        auto conn = (uv_tcp_t*)handle;
        delete conn;
    } else {
        auto conn = (uv_pipe_t*)handle;
        delete conn;
    }
    auto timer = new uv_timer_t;
    uv_timer_init(that->uvloop(), timer);
    timer->data = that;
    uv_timer_start(timer, &LibuvCTransport::do_reconnect, that->intervals_.next(), 0);
}

void LibuvCTransport::do_reconnect(uv_timer_t* handle)
{
    auto that = (LibuvCTransport*)handle->data;
    uv_timer_stop(handle);
    uv_close((uv_handle_t*)handle, [](uv_handle_t* handle) {
        auto timer_handle = (uv_timer_t*)handle;
        delete timer_handle;
    });
    if (!that->init()) {
        that->on_closed_();
    }
}

void LibuvCTransport::on_connected(uv_connect_t* req, int status)
{
    auto that = reinterpret_cast<LibuvCTransport*>(req->data);
    if (status == 0) {
        that->intervals_.reset();
        if (that->stype_ == StreamType::TCP) {
            sockaddr_in addr {};
            int name_len = sizeof(addr);
            int ret = uv_tcp_getsockname(that->tcp_.get(), reinterpret_cast<sockaddr*>(&addr), &name_len);
            if (ret == 0) {
                that->local_port_ = ntohs(addr.sin_port);
                char buffer[64] = { 0 };
                if (inet_ntop(AF_INET, &addr, buffer, 64) == 0) {
                    LOG(WARNING) << "inet_pton failed with " << WSAGetLastError();
                } else {
                    that->local_ip_ = buffer;
                }
            } else {
                LOG(WARNING) << "getsockname failed with " << ret;
            }
        }
        that->on_connected_();
        uv_read_start(that->uvstream(), &LibuvCTransport::on_alloc_memory, &LibuvCTransport::on_read);
    } else {
        LOG(ERR) << "Connect server failed with: " << status;
        that->reconnect();
    }
}

void LibuvCTransport::on_alloc_memory(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf)
{
    (void)handle;
    buf->base = new char[suggested_size];
    buf->len = static_cast<decltype(buf->len)>(suggested_size);
}

void LibuvCTransport::on_read(uv_stream_t* stream, ssize_t nread, const uv_buf_t* uvbuf)
{
    auto that = reinterpret_cast<LibuvCTransport*>(stream->data);
    if (nread == 0) {
        // EAGAIN
        return;
    } else if (nread == UV_EOF) {
        // 读完
        that->reconnect();
    } else if (nread < 0) {
        // 失败，应该断链
        that->reconnect();
    } else {
        // uvbuf.len是容量，nread才是我们想要的，不能用下面这种转法
        // const Buffer* buff = reinterpret_cast<const Buffer*>(uvbuf);
        Buffer buff { uvbuf->base, uint32_t(nread) };
        if (!that->on_read_(buff)) {
            that->reconnect();
        }
    }
}

void LibuvCTransport::on_written(uv_write_t* req, int status)
{
    auto info = (UvWrittenInfo*)req->data;
    auto that = info->that;
    auto user_callback = info->custom_callback;
    delete info;
    delete req;
    // buff交由上层去释放，因为是上层创建的
    user_callback();
    if (status != 0) {
        that->reconnect();
    }
}

void LibuvCTransport::on_dns_resolve(uv_getaddrinfo_t* req, int status, addrinfo* res)
{
    SimpleGuard addrinfo_guard { [res]() {
        uv_freeaddrinfo(res);
    } };
    auto that = reinterpret_cast<LibuvCTransport*>(req->data);
    delete req;
    if (status != 0) {
        LOG(ERR) << "DNS query failed:" << status;
        that->reconnect();
        return;
    }
    addrinfo* addr = res;
    while (addr != nullptr) {
        if (addr->ai_family == AF_INET) {
            break;
        }
        addr = addr->ai_next;
    }
    if (addr == nullptr) {
        LOG(ERR) << "DNS query failed: no ipv4 address";
        that->reconnect();
        return;
    }
    that->tcp_ = std::make_unique<uv_tcp_t>();
    int ret = uv_tcp_init(that->uvloop(), that->tcp_.get());
    if (ret != 0) {
        LOG(ERR) << "Init tcp socket failed: " << ret;
        that->tcp_.reset(); // reset是为了告诉析构函数，不要close这个socket
        that->reconnect(); // 这个状态下，程序重试也无法继续进行下去，是否应该on_close?
        return;
    }
    that->tcp_->data = that;
    that->conn_req_ = std::make_unique<uv_connect_t>();
    that->conn_req_->data = that;
    struct sockaddr_in* addr4 = reinterpret_cast<sockaddr_in*>(addr->ai_addr);
    addr4->sin_port = htons(that->port_);
    ret = uv_tcp_connect(that->conn_req_.get(), that->tcp_.get(), addr->ai_addr, &LibuvCTransport::on_connected);
    if (ret != 0) {
        LOG(ERR) << "Connect to server failed: " << ret;
        that->reconnect();
    }
}

} // namespace ltlib