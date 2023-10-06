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

#include <modules/p2p/p2p.h>

#include <sstream>

#include <g3log/g3log.hpp>

#include <modules/network/network_channel.h>
#include <modules/p2p/netcard.h>

namespace rtc2 {

P2P::P2P(const Params& params)
    : is_server_{params.is_server}
    , network_channel_{params.network_channel}
    , stun_{params.stun}
    , relay_addr_{params.relay}
    , relay_username_{params.relay_username}
    , relay_password_{params.relay_password}
    , password_{params.password}
    , username_{params.username}
    , on_endpoint_info_gathered_{params.on_endpoint_info_gathered}
    , on_error_{params.on_error}
    , on_conn_changed_{params.on_conn_changed}
    , on_read_{params.on_read} {
    ::srand(static_cast<unsigned int>(::time(nullptr)));
}

P2P::~P2P() {}

int32_t P2P::send(std::vector<std::span<const uint8_t>> spans) {
    if (connected_ep_ == nullptr) {
        return -1;
    }
    return connected_ep_->send(spans);
}

void P2P::maybe_start() {
    if (already_started_) {
        return;
    }
    already_started_ = true;
    do_start();
}

void P2P::add_remote_info(const EndpointInfo& info) {
    switch (info.type) {
    case EndpointType::Lan:
        if (lan_ != nullptr) {
            lan_->add_remote_info(info);
        }
        break;
    case EndpointType::Wan:
        LOG(WARNING) << "Unsupported EndpointType " << to_str(info.type).c_str();
        break;
    case EndpointType::Relay:
        LOG(WARNING) << "Unsupported EndpointType " << to_str(info.type).c_str();
        break;
    default:
        LOG(FATAL) << "Unknown EndpointType " << (int)info.type;
        break;
    }
}

void P2P::do_start() {
    create_lan_endpoint();
    create_wan_endpoint();
    if (is_server_) {
        create_relay_endpoint_after_3s();
    }
}

void P2P::post_task(const std::function<void()>& task) {
    auto weak_this = weak_from_this();
    network_channel_->post([task, weak_this]() {
        auto shared_this = weak_this.lock();
        if (shared_this) {
            task();
        }
    });
}

void P2P::post_delayed_task(uint32_t delayed_ms, const std::function<void()>& task) {
    auto weak_this = weak_from_this();
    network_channel_->postDelay(delayed_ms, [task, weak_this]() {
        auto shared_this = weak_this.lock();
        if (shared_this) {
            task();
        }
    });
}

void P2P::create_lan_endpoint() {
    Address netcard_addr = getNetcardAddress();
    if (netcard_addr.family() == -1) {
        LOG(WARNING) << "getNetcardAddress failed, no NIC";
        return;
    }
    LanEndpoint::Params params{};
    params.addr = netcard_addr;
    params.network_channel = network_channel_;
    params.on_connected = std::bind(&P2P::on_connected, this, std::placeholders::_1);
    params.on_endpoint_info = std::bind(&P2P::on_endpoint_info, this, std::placeholders::_1);
    params.on_read = std::bind(&P2P::on_read, this, std::placeholders::_1, std::placeholders::_2,
                               std::placeholders::_3, std::placeholders::_4);
    lan_ = LanEndpoint::create(params);
    if (lan_ == nullptr) {
        return;
    }
}

void P2P::create_wan_endpoint() {
    // wan_ = WanEndpoint::create();
}

void P2P::create_relay_endpoint_after_3s() {
    // network_channel_->postDelay(3000 /*ms*/, std::bind(&P2P::create_relay_endpoint, this));
}

void P2P::create_relay_endpoint() {
    // if (connected_) {
    //     return;
    // }
    // relay_ = RelayEndpoint::create();
}

void P2P::on_endpoint_info(const EndpointInfo& info) {
    on_endpoint_info_gathered_(info);
}

void P2P::on_read(Endpoint* ep, const uint8_t* data, uint32_t size, int64_t time_us) {
    (void)ep;
    on_read_(data, size, time_us);
}

void P2P::on_connected(Endpoint* ep) {
    if (connected_ep_ == nullptr) {
        LOGF(INFO, "First time connected, %s <--> %s", ep->local_info().address.to_string().c_str(),
             ep->remote_info().address.to_string().c_str());
        // TODO: 记录连接时间
        connected_ep_ = ep;
        on_conn_changed_(ep->local_info(), ep->remote_info(), 0);
    }
    else if (connected_ep_->type() != ep->type()) {
        LOGF(INFO, "Connection changed to %s <--> %s", ep->local_info().address.to_string().c_str(),
             ep->remote_info().address.to_string().c_str());
        // 上层没有处理这个conn_changed的逻辑，暂时不回调
        connected_ep_ = ep;
    }
    else {
        // 同一个类型两次on_connected？bug！
        LOG(FATAL) << "Should not reach here";
    }
}

} // namespace rtc2