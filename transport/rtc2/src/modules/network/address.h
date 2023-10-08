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

#if defined(LT_WINDOWS) || defined(_WIN32)
#include <WinSock2.h>
#include <in6addr.h>
#include <ws2ipdef.h>
#else // if defined(LT_WINDOWS) || defined(_WIN32)
#pragma warning unsupported platform
#endif // if defined(LT_WINDOWS) || defined(_WIN32)

#include <string>

namespace rtc2 {

class IPv4 {
public:
    IPv4() = default;
    IPv4(in_addr ip);
    IPv4(const std::string& ipstr);
    bool operator==(const IPv4& other) const;
    const in_addr& to_in_addr() const;
    std::string to_string() const;
    bool is_loopback() const;

private:
    in_addr ip_{};
};

class IPv6 {
public:
    IPv6() = default;
    IPv6(in6_addr ip);
    IPv6(const std::string& ipstr);
    bool operator==(const IPv6& other) const;
    const in6_addr& to_in6_addr() const;
    std::string to_string() const;
    bool is_loopback() const;

private:
    in6_addr ip_{};
};

class Address {
public:
    Address() = default;
    Address(IPv4 ip, uint16_t port);
    Address(in_addr ip, uint16_t port);
    Address(const IPv6& ip, uint16_t port);
    Address(const in6_addr& ip, uint16_t port);
    explicit Address(const sockaddr_in& addr);
    explicit Address(const sockaddr_in6& addr);

    bool operator==(const Address& other) const;
    bool operator!=(const Address& other) const;
    std::string to_string() const;
    std::string ip_to_string() const;

    uint16_t port() const;
    in_addr ipv4() const;
    in6_addr ipv6() const;
    int family() const;

    void set_ip(in_addr ip);
    void set_ip(in6_addr ip);
    void set_port(uint16_t port);

    bool is_private() const;
    bool is_loopback() const;
    bool is_linklocal() const;
    bool is_private_network() const;
    bool is_shared_network() const;

    sockaddr_storage& to_storage(sockaddr_storage& storage) const;
    sockaddr_storage to_storage() const;
    static Address from_storage(const sockaddr_storage& storage);
    static Address from_storage(const sockaddr_storage* storage);
    static Address from_sockaddr(const sockaddr* sockaddr);
    static Address from_str(const std::string& str);

private:
    int family_ = -1;
    uint16_t port_ = 0;
    union IP {
        IPv4 v4;
        IPv6 v6;
        IP()
            : v6{} {}
        IP(in_addr ip)
            : v4(ip) {}
        IP(in6_addr ip)
            : v6(ip) {}
    };
    IP ip_;
};

} // namespace rtc2