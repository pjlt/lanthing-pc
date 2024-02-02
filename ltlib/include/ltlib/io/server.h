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
#include <cstdint>
#include <string>

#include <google/protobuf/message_lite.h>

#include <ltlib/io/ioloop.h>
#include <ltlib/io/types.h>

namespace ltlib {

class ServerImpl;

// 不支持TLS
class Server {
public:
    struct Params {
        StreamType stype;
        IOLoop* ioloop;
        std::string pipe_name;
        std::string bind_ip;
        uint16_t bind_port;
        std::function<void(uint32_t)> on_accepted;
        std::function<void(uint32_t)> on_closed;
        std::function<void(uint32_t /*fd*/, uint32_t /*type*/,
                           const std::shared_ptr<google::protobuf::MessageLite>&)>
            on_message;
    };

public:
    static std::unique_ptr<Server> create(const Params& params);
    bool send(uint32_t fd, uint32_t type, const std::shared_ptr<google::protobuf::MessageLite>& msg,
              const std::function<void()>& callback = nullptr);
    bool send(uint32_t fd, const std::shared_ptr<uint8_t>& data, uint32_t len,
              const std::function<void()>& callback = nullptr);
    // 当上层调用send()返回false时，由上层调用close()关闭这个fd。此时on_closed将被回调
    void close(uint32_t fd);
    std::string ip();
    uint16_t port();

private:
    std::shared_ptr<ServerImpl> impl_;
};

} // namespace ltlib