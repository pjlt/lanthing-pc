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

#include "network_channel.h"

#include <cassert>

#include <ltlib/logging.h>
#include <uv.h>

#include <modules/p2p/p2p.h>

namespace rtc2 {

// static void printNetworkAdapters() {
//     uv_interface_address_t* info;
//     int count;
//     char buf[512];
//     uv_interface_addresses(&info, &count);
//     for (int i = 0; i < count; i++) {
//         uv_interface_address_t interface_a = info[i];
//         if (interface_a.address.address4.sin_family == AF_INET) {
//
//             uv_ip4_name(&interface_a.address.address4, buf, sizeof(buf));
//
//             printf("IPv4 address: %s\n", buf);
//         }
//
//         else if (interface_a.address.address4.sin_family == AF_INET6) {
//
//             uv_ip6_name(&interface_a.address.address6, buf, sizeof(buf));
//
//             printf("IPv6 address: %s\n", buf);
//         }
//         LOG(INFO) << interface_a.name << " " << interface_a.is_internal << " " << buf << " "
//                   << std::string(&interface_a.phys_addr[0], 6).c_str();
//     }
// }

NetworkChannel::NetworkChannel(const Params& p)
    : on_error_{p.on_error}
    , on_endpoint_info_gathered_{p.on_endpoint_info_gathered} {
    // printNetworkAdapters();
    P2P::Params params{};
    params.is_server = p.is_server;
    params.network_channel = this;
    params.password = p.password;
    params.username = p.username;
    params.relay = p.relay;
    params.relay_username = p.relay_username;
    params.relay_password = p.relay_password;
    params.on_error = std::bind(&NetworkChannel::onP2PError, this, std::placeholders::_1);
    params.on_endpoint_info_gathered =
        std::bind(&NetworkChannel::onP2PEndpointInfoGathered, this, std::placeholders::_1);
    params.on_read = std::bind(&NetworkChannel::onP2PRead, this, std::placeholders::_1,
                               std::placeholders::_2, std::placeholders::_3);
    params.on_conn_changed =
        std::bind(&NetworkChannel::onP2PConnchanged, this, std::placeholders::_1,
                  std::placeholders::_2, std::placeholders::_3);
    p2p_ = std::make_shared<P2P>(params);
}

std::unique_ptr<NetworkChannel> NetworkChannel::create(const Params& params) {
    auto ioloop = ltlib::IOLoop::create();
    if (ioloop == nullptr) {
        return nullptr;
    }
    std::unique_ptr<NetworkChannel> channel{new NetworkChannel{params}};
    channel->ioloop_ = std::move(ioloop);
    channel->thread_ = ltlib::BlockingThread::create(
        "rtc2_net", [that = channel.get()](const std::function<void()>& i_am_alive) {
            that->mainLoop(i_am_alive);
        });
    return channel;
}

// 跑在用户线程
bool NetworkChannel::start() {
    if (on_read_ == nullptr || on_conn_changed_ == nullptr) {
        LOG(FATAL) << "NetworkChannel::start failed, callback not set";
        return false;
    }
    post([this]() { p2p_->maybe_start(); });
    return true;
}

void NetworkChannel::setOnRead(
    const std::function<void(const uint8_t*, uint32_t, int64_t)>& on_read) {
    on_read_ = on_read;
}

void NetworkChannel::setOnConnChanged(
    const std::function<void(const EndpointInfo& local, const EndpointInfo& remote,
                             int64_t used_time_ms)>& on_conn_changed) {
    on_conn_changed_ = on_conn_changed;
}

// 跑在用户线程 | 跑在网络线程
void NetworkChannel::addRemoteInfo(const EndpointInfo& info) {
    if (ioloop_->isNotCurrentThread()) {
        post(std::bind(&NetworkChannel::addRemoteInfo, this, info));
        return;
    }
    p2p_->add_remote_info(info);
}

int32_t NetworkChannel::sendPacket(std::vector<std::span<const uint8_t>> spans) {
    return p2p_->send(spans);
}

void NetworkChannel::mainLoop(const std::function<void()>& i_am_alive) {
    LOG(INFO) << "NetworkChannel enter main loop";
    ioloop_->run(i_am_alive);
    LOG(INFO) << "NetworkChannel exit main loop";
}

void NetworkChannel::onP2PError(int32_t error) {
    on_error_(error);
}

void NetworkChannel::onP2PEndpointInfoGathered(const EndpointInfo& info) {
    on_endpoint_info_gathered_(info);
}

void NetworkChannel::onP2PRead(const uint8_t* data, uint32_t size, int64_t time_us) {
    on_read_(data, size, time_us);
}

void NetworkChannel::onP2PConnchanged(const EndpointInfo& local, const EndpointInfo& remote,
                                      int64_t used_time_ms) {
    on_conn_changed_(local, remote, used_time_ms);
}

void NetworkChannel::post(const std::function<void()>& task) {
    std::lock_guard lock{mutex_};
    if (ioloop_) {
        ioloop_->post(task);
    }
}

void NetworkChannel::postDelay(uint32_t delay_ms, const std::function<void()>& task) {
    std::lock_guard lock{mutex_};
    if (ioloop_) {
        ioloop_->postDelay(delay_ms, task);
    }
}

std::unique_ptr<UDPSocket> NetworkChannel::createUDPSocket(const Address& bind_addr) {
    assert(ioloop_->isCurrentThread());
    return UDPSocket::create(ioloop_.get(), bind_addr);
}

} // namespace rtc2