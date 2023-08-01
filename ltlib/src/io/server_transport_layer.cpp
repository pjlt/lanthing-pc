#include "server_transport_layer.h"
#include <g3log/g3log.hpp>

namespace
{

struct UvWrittenInfo
{
    UvWrittenInfo(ltlib::LibuvSTransport::Conn* _conn, const std::function<void()>& cb)
        : conn(_conn)
        , custom_callback(cb)
    {
    }
    ltlib::LibuvSTransport::Conn* conn;
    std::function<void()> custom_callback;
};

} // namespace

namespace ltlib
{

LibuvSTransport::LibuvSTransport(const Params& params)
    : stype_ { params.stype }
    , ioloop_ { params.ioloop }
    , pipe_name_ { params.pipe_name }
    , bind_ip_ { params.bind_ip }
    , bind_port_ { params.bind_port }
    , on_accepted_ { params.on_accepted }
    , on_closed_ { params.on_closed }
    , on_read_ { params.on_read }
{
}

LibuvSTransport::~LibuvSTransport()
{
    if (stype_ == StreamType::TCP) {
        if (server_tcp_ == nullptr) {
            return;
        }
        auto tcp_handle = server_tcp_.release();
        if (ioloop_->is_not_current_thread()) {
            ioloop_->post([tcp_handle]() {
                uv_close((uv_handle_t*)tcp_handle, [](uv_handle_t* handle) { delete (uv_tcp_t*)handle; });
            });
        } else {
            uv_close((uv_handle_t*)tcp_handle, [](uv_handle_t* handle) { delete (uv_tcp_t*)handle; });
        }

    } else {
        if (server_pipe_ == nullptr) {
            return;
        }
        auto pipe_conn = server_pipe_.release();
        if (ioloop_->is_not_current_thread()) {
            ioloop_->post([pipe_conn]() {
                uv_close((uv_handle_t*)pipe_conn, [](uv_handle_t* handle) { delete (uv_pipe_t*)handle; });
            });
        } else {
            uv_close((uv_handle_t*)pipe_conn, [](uv_handle_t* handle) { delete (uv_pipe_t*)handle; });
        }
    }
}

bool LibuvSTransport::init()
{
    if (stype_ == StreamType::TCP) {
        return init_tcp();
    } else {
        return init_pipe();
    }
}

bool LibuvSTransport::send(uint32_t fd, Buffer buff[], uint32_t buff_count, const std::function<void()>& callback)
{
    auto iter = conns_.find(fd);
    if (iter == conns_.cend() || iter->second->closing) {
        LOG(WARNING) << "Can't write to closed connections";
        return false;
    }
    auto conn = iter->second;
    auto info = new UvWrittenInfo(conn.get(), callback);
    uv_write_t* write_req = new uv_write_t {};
    write_req->data = info;
    uv_buf_t* uvbuf = reinterpret_cast<uv_buf_t*>(buff);
    int ret = uv_write(write_req, conn->handle, uvbuf, buff_count, &LibuvSTransport::on_written);
    if (ret != 0) {
        LOGF(WARNING, "%s write failed:%d", stype_ == StreamType::TCP ? "TCP" : "Pipe", ret);
        delete info;
        delete write_req;
        return false;
    }
    return true;
}

void LibuvSTransport::on_written(uv_write_t* req, int status)
{
    auto info = reinterpret_cast<UvWrittenInfo*>(req->data);
    auto user_callback = info->custom_callback;
    Conn* conn = info->conn;
    delete info;
    delete req;
    user_callback();
    if (status != 0) {
        auto that = reinterpret_cast<LibuvSTransport*>(info->conn->svr);
        that->close(conn->fd);
    }
}

bool LibuvSTransport::init_tcp()
{
    server_tcp_ = std::make_unique<uv_tcp_t>();
    int ret = uv_tcp_init(uvloop(), server_tcp_.get());
    if (ret != 0) {
        LOG(WARNING) << "Init tcp socket failed: " << ret;
        server_tcp_.reset(); // reset是为了告诉析构函数，不要close这个socket
    }
    struct sockaddr_in addr;
    ret = uv_ip4_addr(bind_ip_.c_str(), bind_port_, &addr);
    if (ret != 0) {
        LOG(WARNING) << "TCP bind to '" << bind_ip_ << ":" << bind_port_ << "' failed: " << ret;
        server_tcp_.reset();
        return false;
    }
    ret = uv_tcp_bind(server_tcp_.get(), reinterpret_cast<const sockaddr*>(&addr), 0);
    if (ret != 0) {
        LOG(WARNING) << "TCP bind to '" << bind_ip_ << ":" << bind_port_ << "' failed: " << ret;
        server_tcp_.reset();
        return false;
    }
    server_tcp_->data = this;
    constexpr int kBacklog = 4;
    ret = uv_listen(reinterpret_cast<uv_stream_t*>(server_tcp_.get()), kBacklog, &LibuvSTransport::on_new_client);
    if (ret != 0) {
        LOG(WARNING) << "Listen on TCP '" << bind_ip_ << ":" << bind_port_ << "' failed: " << ret;
        server_tcp_.reset();
        return false;
    }
    return true;
}

bool LibuvSTransport::init_pipe()
{
    server_pipe_ = std::make_unique<uv_pipe_t>();
    uv_pipe_init(uvloop(), server_pipe_.get(), 0); /*返回值永远是0*/
    int ret = uv_pipe_bind(server_pipe_.get(), pipe_name_.c_str());
    if (ret != 0) {
        LOG(WARNING) << "Pipe bind to name '" << pipe_name_ << "' failed: " << ret;
        server_pipe_.reset();
        return false;
    }
    server_pipe_->data = this;
    constexpr int kBacklog = 4;
    ret = uv_listen(reinterpret_cast<uv_stream_t*>(server_pipe_.get()), kBacklog, &LibuvSTransport::on_new_client);
    if (ret != 0) {
        LOG(WARNING) << "Listen on pipe '" << pipe_name_ << "' failed: " << ret;
        server_pipe_.reset();
        return false;
    }
    return true;
}

void LibuvSTransport::close(uint32_t fd)
{
    auto iter = conns_.find(fd);
    if (iter == conns_.cend() || iter->second->closing) {
        LOG(WARNING) << "Can't close a closed fd:" << fd;
        return;
    }
    std::shared_ptr<Conn> conn = iter->second;
    conn->closing = true;
    on_closed_(fd);
    uv_close(reinterpret_cast<uv_handle_t*>(conn->handle), &LibuvSTransport::on_conn_closed);
}

void LibuvSTransport::on_conn_closed(uv_handle_t* handle)
{
    Conn* conn = reinterpret_cast<Conn*>(handle->data);
    auto that = reinterpret_cast<LibuvSTransport*>(conn->svr);
    that->conns_.erase(conn->fd);
}

uv_loop_t* LibuvSTransport::uvloop()
{
    return reinterpret_cast<uv_loop_t*>(ioloop_->context());
}

uv_stream_t* LibuvSTransport::server_handle()
{
    if (stype_ == StreamType::Pipe) {
        return reinterpret_cast<uv_stream_t*>(server_pipe_.get());
    } else {
        return reinterpret_cast<uv_stream_t*>(server_tcp_.get());
    }
}

void LibuvSTransport::on_new_client(uv_stream_t* server, int status)
{
    auto that = reinterpret_cast<LibuvSTransport*>(server->data);
    if (status != 0) {
        LOG(WARNING) << "New connection error: " << status;
        return;
    }
    std::shared_ptr<Conn> conn = Conn::create(that->stype_, that->uvloop());
    if (conn == nullptr) {
        LOG(WARNING) << "Create connection handle for new connection failed";
        return;
    }
    conn->handle->data = conn.get();
    conn->svr = that;
    int ret = uv_accept(that->server_handle(), conn->handle);
    if (ret != 0) {
        LOG(WARNING) << "Accept pipe client failed: " << ret;
        return;
    }
    conn->fd = that->latest_fd_++;
    that->conns_[conn->fd] = conn;
    ret = uv_read_start(conn->handle, &LibuvSTransport::on_alloc_memory, &LibuvSTransport::on_read);
    if (ret != 0) {
        LOG(WARNING) << "Start read on pipe failed: " << ret;
        that->conns_.erase(conn->fd);
        return;
    }
    that->on_accepted_(conn->fd);
}

void LibuvSTransport::on_alloc_memory(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf)
{
    (void)handle;
    buf->base = new char[suggested_size];
    buf->len = static_cast<decltype(buf->len)>(suggested_size);
}

void LibuvSTransport::on_read(uv_stream_t* stream, ssize_t nread, const uv_buf_t* uvbuf)
{
    auto conn = reinterpret_cast<LibuvSTransport::Conn*>(stream->data);
    auto that = conn->svr;
    if (nread == 0) {
        // EAGAIN
        return;
    } else if (nread == UV_EOF) {
        // 读完
        that->close(conn->fd);
    } else if (nread < 0) {
        // 失败，应该断链
        that->close(conn->fd);
    } else {
        // const Buffer* buff = reinterpret_cast<const Buffer*>(uvbuf);
        Buffer buff { uvbuf->base, uint32_t(nread) };
        if (!that->on_read_(conn->fd, buff)) {
            that->close(conn->fd);
        }
    }
}

LibuvSTransport::Conn::Conn(StreamType _stype)
    : stype { _stype }
{
    if (stype == StreamType::Pipe) {
        handle = reinterpret_cast<uv_stream_t*>(new uv_pipe_t);
    } else {
        handle = reinterpret_cast<uv_stream_t*>(new uv_tcp_t);
    }
}

LibuvSTransport::Conn::~Conn()
{
    if (stype == StreamType::Pipe) {
        delete reinterpret_cast<uv_pipe_t*>(handle);
    } else {
        delete reinterpret_cast<uv_tcp_t*>(handle);
    }
}

std::shared_ptr<LibuvSTransport::Conn> LibuvSTransport::Conn::create(StreamType _stype, uv_loop_t* uvloop)
{
    auto conn = std::make_shared<Conn>(_stype);
    if (_stype == StreamType::Pipe) {
        uv_pipe_init(uvloop, reinterpret_cast<uv_pipe_t*>(conn->handle), 0);
        return conn;
    } else {
        int ret = uv_tcp_init(uvloop, reinterpret_cast<uv_tcp_t*>(conn->handle));
        if (ret != 0) {
            return conn;
        }
        return nullptr;
    }
}

} // namespace ltlib