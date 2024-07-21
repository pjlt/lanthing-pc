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

#include "address.h"

#if defined(LT_WINDOWS) || defined(_WIN32)
// clang-format off
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <MSWSock.h>
// clang-format on
#else
#include <arpa/inet.h>
#include <fcntl.h>
#endif

#include <cstring>

#include <sstream>

namespace {

in_addr to_in_addr(const std::string& str) {
    in_addr addr{};
    inet_pton(AF_INET, str.c_str(), &addr);
    return addr;
}

in6_addr to_in6_addr(const std::string& str) {
    in6_addr addr{};
    inet_pton(AF_INET6, str.c_str(), &addr);
    return addr;
}

} // namespace

namespace rtc2 {

IPv4::IPv4(in_addr ip)
    : ip_{ip} {}

IPv4::IPv4(const std::string& ipstr)
    : ip_{::to_in_addr(ipstr)} {}

bool IPv4::operator==(const IPv4& other) const {
    return memcmp(&ip_, &other.ip_, sizeof(ip_)) == 0;
}

const in_addr& IPv4::to_in_addr() const {
    return ip_;
}

std::string IPv4::to_string() const {
    char buff[INET_ADDRSTRLEN] = {0};
    auto p = ::inet_ntop(AF_INET, &ip_, buff, INET_ADDRSTRLEN);
    return p == nullptr ? "" : std::string{p};
}

bool IPv4::is_loopback() const {
    uint32_t ip = ntohl(ip_.s_addr);
    return (ip >> 24) == 127;
}

IPv6::IPv6(in6_addr ip)
    : ip_{ip} {}

IPv6::IPv6(const std::string& ipstr)
    : ip_{::to_in6_addr(ipstr)} {}

bool IPv6::operator==(const IPv6& other) const {
    return memcmp(&ip_, &other.ip_, sizeof(ip_)) == 0;
}

const in6_addr& IPv6::to_in6_addr() const {
    return ip_;
}

std::string IPv6::to_string() const {
    char buff[INET6_ADDRSTRLEN] = {0};
    auto p = ::inet_ntop(AF_INET6, &ip_, buff, INET6_ADDRSTRLEN);
    return p == nullptr ? "" : std::string{p};
}

bool IPv6::is_loopback() const {
    return ::memcmp(&ip_, &in6addr_loopback, sizeof(ip_)) == 0;
}

Address::Address(IPv4 ip, uint16_t port)
    : family_(AF_INET)
    , port_(port)
    , ip_(ip.to_in_addr()) {}

Address::Address(in_addr ip, uint16_t port)
    : family_(AF_INET)
    , port_(port)
    , ip_(ip) {}

Address::Address(const IPv6& ip, uint16_t port)
    : family_(AF_INET6)
    , port_(port)
    , ip_(ip.to_in6_addr()) {}

Address::Address(const in6_addr& ip, uint16_t port)
    : family_(AF_INET6)
    , port_(port)
    , ip_(ip) {}

Address::Address(const sockaddr_in& addr)
    : family_(AF_INET)
    , port_(addr.sin_port)
    , ip_(addr.sin_addr) {}

Address::Address(const sockaddr_in6& addr)
    : family_(AF_INET)
    , port_(addr.sin6_port)
    , ip_(addr.sin6_addr) {}

bool Address::operator==(const Address& other) const {
    if (family_ != other.family_) {
        return false;
    }
    if (port_ != other.port_) {
        return false;
    }
    if (family_ == AF_INET) {
        return ip_.v4 == other.ip_.v4;
    }
    if (family_ == AF_INET6) {
        return ip_.v6 == other.ip_.v6;
    }
    return false;
}

bool Address::operator!=(const Address& other) const {
    return !operator==(other);
}

std::string Address::to_string() const {
    std::stringstream ss;
    if (family_ == AF_INET) {
        ss << ip_.v4.to_string() << ':' << port_;
    }
    else if (family_ == AF_INET6) {
        ss << '[' << ip_.v6.to_string() << ']' << ':' << port_;
    }
    return ss.str();
}

std::string Address::ip_to_string() const {
    switch (family_) {
    case AF_INET:
        return ip_.v4.to_string();
    case AF_INET6:
        return ip_.v6.to_string();
    default:
        return "";
    }
}

uint16_t Address::port() const {
    return port_;
}

in_addr Address::ipv4() const {
    return ip_.v4.to_in_addr();
}

in6_addr Address::ipv6() const {
    return ip_.v6.to_in6_addr();
}

int Address::family() const {
    return family_;
}

void Address::set_ip(in_addr ip) {
    family_ = AF_INET;
    ip_.v4 = ip;
}

void Address::set_ip(in6_addr ip) {
    family_ = AF_INET6;
    ip_.v6 = ip;
}

void Address::set_port(uint16_t port) {
    port_ = port;
}

bool Address::is_private() const {
    return is_linklocal() || is_loopback() || is_private_network() || is_shared_network();
}

bool Address::is_loopback() const {
    if (family_ == AF_INET) {
        return ip_.v4.is_loopback();
    }
    else if (family_ == AF_INET6) {
        return ip_.v6.is_loopback();
    }
    return false;
}

bool Address::is_linklocal() const {
    if (family_ == AF_INET) {
        uint32_t ip = ntohl(ip_.v4.to_in_addr().s_addr);
        return ((ip >> 16) == ((169 << 8) | 254));
    }
    else if (family_ == AF_INET6) {
        return (ip_.v6.to_in6_addr().s6_addr[0] == 0xFE) &&
               ((ip_.v6.to_in6_addr().s6_addr[1] & 0xC0) == 0x80);
    }
    return false;
}

// 192.168.xx.xx
// 172.[16-31].xx.xx
// 10.xx.xx.xx
// fd:xx...
bool Address::is_private_network() const {
    if (family_ == AF_INET) {
        uint32_t ip = ntohl(ip_.v4.to_in_addr().s_addr);
        return ((ip >> 24) == 10) || ((ip >> 20) == ((172 << 4) | 1)) ||
               ((ip >> 16) == ((192 << 8) | 168));
    }
    else if (family_ == AF_INET6) {
        return ip_.v6.to_in6_addr().s6_addr[0] == 0xFD;
    }
    return false;
}

// 100.64.xx.xx
bool Address::is_shared_network() const {
    if (family_ != AF_INET) {
        return false;
    }
    uint32_t ip = ntohl(ip_.v4.to_in_addr().s_addr);
    return (ip >> 22) == ((100 << 2) | 1);
}

sockaddr_storage& Address::to_storage(sockaddr_storage& storage) const {
    if (family_ == AF_INET) {
        sockaddr_in* addr = reinterpret_cast<sockaddr_in*>(&storage);
        addr->sin_family = AF_INET;
        addr->sin_port = htons(port_);
        addr->sin_addr = ip_.v4.to_in_addr();
    }
    else if (family_ == AF_INET6) {
        sockaddr_in6* addr = reinterpret_cast<sockaddr_in6*>(&storage);
        addr->sin6_family = AF_INET6;
        addr->sin6_port = htons(port_);
        addr->sin6_addr = ip_.v6.to_in6_addr();
    }
    return storage;
}

sockaddr_storage Address::to_storage() const {
    sockaddr_storage storage{};
    return to_storage(storage);
}

Address Address::from_storage(const sockaddr_storage& storage) {
    return from_storage(&storage);
}

Address Address::from_storage(const sockaddr_storage* storage) {
    Address addr{};
    if (storage->ss_family == AF_INET) {
        const sockaddr_in* in4 = reinterpret_cast<const sockaddr_in*>(storage);
        addr.family_ = AF_INET;
        addr.port_ = ntohs(in4->sin_port);
        addr.ip_.v4 = in4->sin_addr;
    }
    else if (storage->ss_family == AF_INET6) {
        const sockaddr_in6* in6 = reinterpret_cast<const sockaddr_in6*>(storage);
        addr.family_ = AF_INET6;
        addr.port_ = ntohs(in6->sin6_port);
        addr.ip_.v6 = in6->sin6_addr;
    }
    return addr;
}

Address Address::from_sockaddr(const sockaddr* sockaddr) {
    return from_storage(reinterpret_cast<const sockaddr_storage*>(sockaddr));
}

Address Address::from_str(const std::string& str) {
    if (str.at(0) == '[') {
        std::string::size_type closebracket = str.rfind(']');
        if (closebracket != std::string::npos) {
            std::string::size_type colon = str.find(':', closebracket);
            if (colon != std::string::npos && colon > closebracket) {
                uint16_t port =
                    static_cast<uint16_t>(strtoul(str.substr(colon + 1).c_str(), nullptr, 10));
                std::string ip_str = str.substr(1, closebracket - 1);
                return Address{IPv6{ip_str}, port};
            }
            else {
                return {};
            }
        }
        else {
            return {};
        }
    }
    else {
        std::string::size_type pos = str.find(':');
        if (std::string::npos == pos)
            return {};
        uint16_t port = static_cast<uint16_t>(strtoul(str.substr(pos + 1).c_str(), nullptr, 10));
        std::string ip_str = str.substr(0, pos);
        return Address{IPv4{ip_str}, port};
    }
}

} // namespace rtc2
