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
#include <functional>
#include <memory>

#include <ltlib/threads.h>

#include <transport/transport.h>

namespace lt {

class ClientSession {
public:
    struct Params {
        std::string client_id;
        std::string room_id;
        std::string auth_token;
        std::string p2p_username;
        std::string p2p_password;
        std::string signaling_addr;
        int32_t signaling_port;
        lt::VideoCodecType video_codec_type;
        uint32_t width;
        uint32_t height;
        uint32_t refresh_rate;
        bool enable_gamepad;
        bool enable_driver_input;
        uint32_t audio_channels;
        uint32_t audio_freq;
        std::vector<std::string> reflex_servers;
        std::function<void()> on_exited;
    };

public:
    ClientSession(const Params& params);
    ~ClientSession();
    bool start();
    std::string clientID() const;
    std::string roomID() const;

private:
    void mainLoop(const std::function<void()>& i_am_alive);

private:
    Params params_;
    int64_t process_id_;
    void* handle_;
#if LT_WINDOWS
    std::unique_ptr<ltlib::BlockingThread> thread_;
#elif LT_LINUX
    std::unique_ptr<std::thread> thread_;
#else
#endif
    bool stoped_ = true;
};

} // namespace lt