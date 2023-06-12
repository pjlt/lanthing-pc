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
    std::unique_ptr<uv_tcp_t> server_tcp_;
    std::unique_ptr<uv_pipe_t> server_pipe_;
    std::function<void(uint32_t)> on_accepted_;
    std::function<void(uint32_t)> on_closed_;
    std::function<bool(uint32_t, const Buffer&)> on_read_;
    std::map<uint32_t /*fd*/, std::shared_ptr<Conn>> conns_;
};

} // namespace ltlib