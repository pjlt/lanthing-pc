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

#include "netcard.h"

#if defined(LT_WINDOWS)
#include <WinSock2.h>
#include <iphlpapi.h>
#include <ws2tcpip.h>
#else
#endif

#include <ltlib/logging.h>

namespace rtc2 {

#if defined(LT_WINDOWS)

std::vector<Address> getNetcardAddress() {
    std::vector<Address> result{};
    ULONG flags = (GAA_FLAG_SKIP_DNS_SERVER | GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST |
                   GAA_FLAG_INCLUDE_PREFIX);
    ULONG buffer_size = 16 * 16384;
    std::vector<uint8_t> buffer;
    PIP_ADAPTER_ADDRESSES adapters = nullptr;
    ULONG ret = 0;
    do {
        buffer.resize(buffer_size);
        adapters = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buffer.data());
        ret = GetAdaptersAddresses(AF_INET, flags, nullptr, adapters, &buffer_size);
    } while (ret == ERROR_BUFFER_OVERFLOW);
    if (ret != ERROR_SUCCESS) {
        return result;
    }
    while (adapters != nullptr) {
        if (adapters->OperStatus != IfOperStatusUp) {
            adapters = adapters->Next;
            continue;
        }

        // if (adapters->FirstUnicastAddress) {
        //     sockaddr_in* addr =
        //         reinterpret_cast<sockaddr_in*>(adapters->FirstUnicastAddress->Address.lpSockaddr);
        //     sockaddr_storage storage{};
        //     memcpy(&storage, addr, sizeof(sockaddr_in));
        //     auto toprint = Address::from_storage(storage);
        //     // 昏古7了，WSL的IfType居然是IF_TYPE_ETHERNET_CSMACD
        //     // 只能把所有地址都回调出去，哪个通了用哪个
        //     LOG(INFO) << adapters->IfType << " " << toprint.to_string();
        // }
        if (adapters->IfType != IF_TYPE_ETHERNET_CSMACD &&
            adapters->IfType != IF_TYPE_ETHERNET_3MBIT && adapters->IfType != IF_TYPE_IEEE80212 &&
            adapters->IfType != IF_TYPE_FASTETHER && adapters->IfType != IF_TYPE_FASTETHER_FX &&
            adapters->IfType != IF_TYPE_GIGABITETHERNET && adapters->IfType != IF_TYPE_IEEE80211 &&
            adapters->IfType != IF_TYPE_WWANPP && adapters->IfType != IF_TYPE_WWANPP2) {
            continue;
        }
        PIP_ADAPTER_UNICAST_ADDRESS address = adapters->FirstUnicastAddress;
        if (address != nullptr) {
            sockaddr_in* addr = reinterpret_cast<sockaddr_in*>(address->Address.lpSockaddr);
            sockaddr_storage storage{};
            memcpy(&storage, addr, sizeof(sockaddr_in));
            auto new_addr = Address::from_storage(storage);
            result.push_back(new_addr);
        }
        adapters = adapters->Next;
    }
    return result;
}

#elif defined(LT_LINUX)
std::vector<Address> getNetcardAddress() {}
#else
#pragma error unsupported platform
#endif

} // namespace rtc2