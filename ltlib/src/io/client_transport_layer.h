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
#include <string>
#include <uv.h>
#include <ltlib/reconnect_interval.h>
#include <ltlib/io/ioloop.h>
#include <ltlib/io/types.h>
#include "buffer.h"

namespace ltlib
{

class CTransport
{
public:
    struct Params
    {
        StreamType stype;
        IOLoop* ioloop;
        std::string pipe_name;
        std::string host;
        uint16_t port;
        std::string cert;
        std::function<bool()> on_connected;
        std::function<void()> on_closed;
        std::function<void()> on_reconnecting;
        std::function<bool(const Buffer&)> on_read;
    };

public:
    virtual ~CTransport() { }
    virtual bool init() = 0;
    virtual bool send(Buffer buff[], uint32_t buff_count, const std::function<void()>& callback) = 0;
    virtual void reconnect() = 0;
};

class LibuvCTransport : public CTransport
{
public:
    LibuvCTransport(const Params& params);
    ~LibuvCTransport() override;
    bool init() override;
    bool send(Buffer buff[], uint32_t buff_count, const std::function<void()>& callback) override;
    void reconnect() override;
    bool is_tcp() const;
    const std::string& pipe_name();
    const std::string& host();
    std::string ip() const;
    uint16_t port() const;

private:
    bool init_tcp();
    bool init_pipe();
    uv_loop_t* uvloop();
    uv_stream_t* uvstream();
    uv_handle_t* uvhandle();
    uv_handle_t* uvhandle_release();
    static void delay_reconnect(uv_handle_t* handle);
    static void do_reconnect(uv_timer_t* handle);
    static void on_connected(uv_connect_t* req, int status);
    static void on_alloc_memory(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf);
    static void on_read(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf);
    static void on_written(uv_write_t* req, int status);
    static void on_dns_resolve(uv_getaddrinfo_t* req, int status, struct addrinfo* res);

private:
    StreamType stype_;
    IOLoop* ioloop_;
    std::string pipe_name_;
    std::string host_;
    uint16_t port_;
    std::string local_ip_;
    uint16_t local_port_ = 0;
    std::unique_ptr<uv_tcp_t> tcp_;
    std::unique_ptr<uv_pipe_t> pipe_;
    std::unique_ptr<uv_connect_t> conn_req_;
    std::function<void()> on_connected_;
    std::function<void()> on_closed_;
    std::function<void()> on_reconnecting_;
    std::function<bool(const Buffer&)> on_read_;
    ltlib::ReconnectInterval intervals_;
};

} // namespace ltlib