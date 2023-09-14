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

#include <ltlib/io/client.h>
#include <g3log/g3log.hpp>
#include <ltproto/ltproto.h>
#include "client_transport_layer.h"
#include "client_secure_layer.h"

namespace ltlib
{

class ClientImpl
{
public:
    ClientImpl(const Client::Params& params);
    ~ClientImpl();
    bool init();
    bool send(uint32_t type, const std::shared_ptr<google::protobuf::MessageLite>& msg, const std::function<void()>& callback);
    bool send(const std::shared_ptr<uint8_t>& data, uint32_t len, const std::function<void()>& callback);
    void reconnect();

private:
    CTransport::Params make_transport_params(const Client::Params& cparams);
    bool on_transport_connected();
    void on_transport_closed();
    void on_transport_reconnecting();
    bool on_transport_read(const Buffer& buff);

private:
    bool connected_ = false;
    IOLoop* ioloop_;
    std::function<void()> on_connected_;
    std::function<void()> on_closed_;
    std::function<void()> on_reconnecting_;
    std::function<void(uint32_t type, const std::shared_ptr<google::protobuf::MessageLite>&)> on_message_;
    std::unique_ptr<CTransport> transport_;
    ltproto::Parser parser_;
};

ClientImpl::ClientImpl(const Client::Params& params)
    : ioloop_ { params.ioloop }
    , on_connected_ { params.on_connected }
    , on_closed_ { params.on_closed }
    , on_reconnecting_ { params.on_reconnecting }
    , on_message_ { params.on_message }
{
    if (params.is_tls) {
        transport_ = std::make_unique<MbedtlsCTransport>(make_transport_params(params));
    } else {
        transport_ = std::make_unique<LibuvCTransport>(make_transport_params(params));
    }
}

ClientImpl::~ClientImpl()
{
    //
}

CTransport::Params ClientImpl::make_transport_params(const Client::Params& cparams)
{
    CTransport::Params tparams {};
    tparams.stype = cparams.stype;
    tparams.ioloop = cparams.ioloop;
    tparams.pipe_name = cparams.pipe_name;
    tparams.host = cparams.host;
    tparams.port = cparams.port;
    tparams.cert = cparams.cert;
    tparams.on_connected = std::bind(&ClientImpl::on_transport_connected, this);
    tparams.on_closed = std::bind(&ClientImpl::on_transport_closed, this);
    tparams.on_reconnecting = std::bind(&ClientImpl::on_transport_reconnecting, this);
    tparams.on_read = std::bind(&ClientImpl::on_transport_read, this, std::placeholders::_1);
    return tparams;
}

bool ClientImpl::init()
{
    return transport_->init();
}

bool ClientImpl::on_transport_connected()
{
    connected_ = true;
    on_connected_();
    return true;
}

void ClientImpl::on_transport_closed()
{
    connected_ = false;
    on_closed_();
}

void ClientImpl::on_transport_reconnecting()
{
    connected_ = false;
    parser_.clear();
    on_reconnecting_();
}

bool ClientImpl::on_transport_read(const Buffer& buff)
{
    parser_.push_buffer(reinterpret_cast<const uint8_t*>(buff.base), buff.len);
    if (!parser_.parse_buffer()) {
        return false;
    }
    while (auto msg = parser_.pop_message()) {
        on_message_(msg->type, msg->msg);
    }
    return true;
}

bool ClientImpl::send(uint32_t type, const std::shared_ptr<google::protobuf::MessageLite>& msg, const std::function<void()>& callback)
{
    if (!ioloop_->isCurrentThread()) {
        LOG(FATAL) << "Send data in wrong thread!";
        return false;
    }
    if (!connected_) {
        return false;
    }
    auto packet = ltproto::Packet::create({ type, msg }, true);
    if (!packet.has_value()) {
        LOG(WARNING) << "Create net packet failed, type:" << type;
        return false;
    }
    const auto& pkt = packet.value();
    Buffer buff[2] = {
        { (char*)&pkt.header, sizeof(pkt.header) },
        { (char*)pkt.payload.get(), pkt.header.payload_size }
    };
    return transport_->send(buff, 2, [packet, callback]() {
        // 把packet capture进来，是为了延续内部shared_ptr的生命周期
        if (callback != nullptr) {
            callback();
        }
    });
}

bool ClientImpl::send(const std::shared_ptr<uint8_t>& data, uint32_t len, const std::function<void()>& callback)
{
    if (!ioloop_->isCurrentThread()) {
        LOG(FATAL) << "Send data in wrong thread!";
        return false;
    }
    if (!connected_) {
        return false;
    }
    auto packet = ltproto::Packet::create(data, len, true);
    if (!packet.has_value()) {
        LOG(WARNING) << "Create net packet failed";
        return false;
    }
    const auto& pkt = packet.value();
    Buffer buff[2] = {
        { (char*)&pkt.header, sizeof(pkt.header) },
        { (char*)pkt.payload.get(), pkt.header.payload_size }
    };
    return transport_->send(buff, 2, [packet, callback]() {
        // 把packet capture进来，是为了延续内部shared_ptr的生命周期
        if (callback != nullptr) {
            callback();
        }
    });
}

void ClientImpl::reconnect()
{
    transport_->reconnect();
}

std::unique_ptr<Client> Client::create(const Params& params)
{
    auto impl = std::make_shared<ClientImpl>(params);
    if (!impl->init()) {
        return nullptr;
    }
    std::unique_ptr<Client> client { new Client };
    client->impl_ = std::move(impl);
    return client;
}

bool Client::send(uint32_t type, const std::shared_ptr<google::protobuf::MessageLite>& msg, const std::function<void()>& callback)
{
    return impl_->send(type, msg, callback);
}

bool Client::send(const std::shared_ptr<uint8_t>& data, uint32_t len, const std::function<void()>& callback)
{
    return impl_->send(data, len, callback);
}

void Client::reconnect()
{
    impl_->reconnect();
}

} // namespace ltlib