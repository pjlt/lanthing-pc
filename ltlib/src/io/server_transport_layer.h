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
#include <ltlib/io/types.h>
#include <ltlib/io/ioloop.h>
#include "buffer.h"
#include <cstdint>
#include <functional>
#include <string>
#include <map>
#include <optional>
#include <uv.h>

namespace ltlib
{

class LibuvSTransport
{
public:
    struct Params
    {
        StreamType stype;
        IOLoop* ioloop;
        std::string pipe_name;
        std::string bind_ip;
        uint16_t bind_port;
        std::function<void(uint32_t)> on_accepted;
        std::function<void(uint32_t)> on_closed;
        std::function<bool(uint32_t, const Buffer&)> on_read;
    };
    struct Conn
    {
        Conn(StreamType _stype);
        ~Conn();
        static std::shared_ptr<Conn> create(StreamType _stype, uv_loop_t* uvloop);
        uint32_t fd;
        StreamType stype;
        uv_stream_t* handle;
        LibuvSTransport* svr;
        bool closing = false;
    };

public:
    LibuvSTransport(const Params& params);
    ~LibuvSTransport();
    bool init();
    bool send(uint32_t fd, Buffer buff[], uint32_t buff_count, const std::function<void()>& callback);
    void close(uint32_t fd);
    std::string ip() const;
    uint16_t port() const;

private:
    bool init_tcp();
    bool init_pipe();
    uv_loop_t* uvloop();
    uv_stream_t* server_handle();
    static void on_new_client(uv_stream_t* server, int status);
    static void on_alloc_memory(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf);
    static void on_read(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf);
    static void on_conn_closed(uv_handle_t* handle);
    static void on_written(uv_write_t* req, int status);

private:
    uint32_t latest_fd_ = 0;
    StreamType stype_;
    IOLoop* ioloop_;
    std::string pipe_name_;
    std::string bind_ip_;
    uint16_t bind_port_;
    uint16_t listen_port_;
    std::unique_ptr<uv_tcp_t> server_tcp_;
    std::unique_ptr<uv_pipe_t> server_pipe_;
    std::function<void(uint32_t)> on_accepted_;
    std::function<void(uint32_t)> on_closed_;
    std::function<bool(uint32_t, const Buffer&)> on_read_;
    std::map<uint32_t /*fd*/, std::shared_ptr<Conn>> conns_;
};

} // namespace ltlib