#include <ltlib/io/server.h>
#include <g3log/g3log.hpp>
#include <ltproto/ltproto.h>
#include "server_transport_layer.h"

namespace
{

struct Conn
{
    Conn()
        : fd { std::numeric_limits<uint32_t>::max() }
    {
    }
    explicit Conn(uint32_t _fd)
        : fd{ _fd }
        , parser{ std::make_shared<ltproto::Parser>() }
    {
    }
    uint32_t fd;
    std::shared_ptr<ltproto::Parser> parser;
};

} // namespace


namespace ltlib
{

class ServerImpl
{
public:
    ServerImpl(const Server::Params& params);
    bool init();
    bool send(uint32_t fd, uint32_t type, const std::shared_ptr<google::protobuf::MessageLite>& msg, const std::function<void()>& callback);
    void close(uint32_t fd);

private:
    LibuvSTransport::Params make_uv_params(const Server::Params& params);
    void on_transport_accepted(uint32_t fd);
    void on_transport_closed(uint32_t fd);
    bool on_transport_read(uint32_t fd, const Buffer& buff);

private:
    std::unique_ptr<LibuvSTransport> transport_;
    std::function<void(uint32_t)> on_accepted_;
    std::function<void(uint32_t)> on_closed_;
    std::function<void(uint32_t/*fd*/, uint32_t/*type*/, const std::shared_ptr<google::protobuf::MessageLite>&)> on_message_;
    std::map<uint32_t/*fd*/, Conn> conns_;
};

ServerImpl::ServerImpl(const Server::Params& params)
    : transport_{ std::make_unique<LibuvSTransport>(make_uv_params(params)) }
    , on_accepted_{ params.on_accepted }
    , on_closed_{ params.on_closed }
    , on_message_{ params.on_message }
{
}

LibuvSTransport::Params ServerImpl::make_uv_params(const Server::Params& params)
{
    LibuvSTransport::Params uvparams{};
    uvparams.stype = params.stype;
    uvparams.ioloop = params.ioloop;
    uvparams.pipe_name = params.pipe_name;
    uvparams.bind_ip = params.bind_ip;
    uvparams.bind_port = params.bind_port;
    uvparams.on_accepted = std::bind(&ServerImpl::on_transport_accepted, this, std::placeholders::_1);
    uvparams.on_closed = std::bind(&ServerImpl::on_transport_closed, this, std::placeholders::_1);
    uvparams.on_read = std::bind(&ServerImpl::on_transport_read, this, std::placeholders::_1, std::placeholders::_2);
    return uvparams;
}

bool ServerImpl::init()
{
    return transport_->init();
}

bool ServerImpl::send(uint32_t fd, uint32_t type, const std::shared_ptr<google::protobuf::MessageLite>& msg, const std::function<void()>& callback)
{
    auto iter = conns_.find(fd);
    if (iter == conns_.cend()) {
        LOG(WARNING) << "Send data to invalid fd:" << fd;
        return false;
    }
    auto packet = ltproto::Packet::create({ type, msg }, true);
    if (!packet.has_value()) {
        LOG(WARNING) << "Create net packet failed, type:" << type;
        return false;
    }
    const auto& pkt = packet.value();
    Buffer buffs[2] = {
        {(char*)&pkt.header, sizeof(pkt.header)},
        {(char*)pkt.payload.get(), pkt.header.payload_size}
    };
    return transport_->send(fd, buffs, 2, [packet, callback]() {
        //把packet capture进来，是为了延续内部shared_ptr的生命周期
        if (callback != nullptr) {
            callback();
        }
    });
}

void ServerImpl::close(uint32_t fd)
{
    transport_->close(fd);
}

void ServerImpl::on_transport_accepted(uint32_t fd)
{
    Conn conn{ fd };
    conns_[fd] = conn;
    on_accepted_(fd);
}

void ServerImpl::on_transport_closed(uint32_t fd)
{
    on_closed_(fd);
    conns_.erase(fd);
}

bool ServerImpl::on_transport_read(uint32_t fd, const Buffer& buff)
{
    auto iter = conns_.find(fd);
    if (iter == conns_.cend()) {
        LOG(WARNING) << "Read data on invalid fd:" << fd;
        return false;
    }
    auto conn = iter->second;
    conn.parser->push_buffer(reinterpret_cast<const uint8_t*>(buff.base), buff.len);
    if (!conn.parser->parse_buffer()) {
        LOG(WARNING) << "Parse data failed";
        return false;
    }
    while (auto msg = conn.parser->pop_message()) {
        on_message_(fd, msg.value().type, msg.value().msg);
    }
}

std::unique_ptr<Server> Server::create(const Server::Params& params)
{
    auto impl = std::make_shared<ServerImpl>(params);
    if (!impl->init()) {
        return nullptr;
    }
    std::unique_ptr<Server> server{ new Server };
    server->impl_ = std::move(impl);
    return server;
}

bool Server::send(uint32_t fd, uint32_t type, const std::shared_ptr<google::protobuf::MessageLite>& msg, const std::function<void()>& callback)
{
    return impl_->send(fd, type, msg, callback);
}

void Server::close(uint32_t fd)
{
    impl_->close(fd);
}

} // namespace ltlib