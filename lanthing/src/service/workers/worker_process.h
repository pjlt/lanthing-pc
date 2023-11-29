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

#include <atomic>
#include <future>
#include <memory>
#include <mutex>
#include <string>

#include <google/protobuf/message_lite.h>

#include <ltlib/io/ioloop.h>
#include <ltlib/io/server.h>
#include <ltlib/threads.h>
#include <ltproto/ltproto.h>
#include <transport/transport.h>

namespace lt {

namespace svc {

class WorkerProcess {
public:
    struct Params {
        std::string pipe_name;
        std::string path;
        uint32_t client_width;
        uint32_t client_height;
        uint32_t client_refresh_rate;
        std::vector<lt::VideoCodecType> client_codecs;
    };

public:
    static std::unique_ptr<WorkerProcess> create(const Params& params);
    ~WorkerProcess();

private:
    WorkerProcess(const Params& params);
    void start();
    void mainLoop(const std::function<void()>& i_am_alive);
    bool launchWorkerProcess();
    void waitForWorkerProcess(const std::function<void()>& i_am_alive);

private:
    std::function<void(uint32_t, std::shared_ptr<google::protobuf::MessageLite>)> on_message_;
    std::string path_;
    std::string pipe_name_;
    uint32_t client_width_;
    uint32_t client_height_;
    uint32_t client_refresh_rate_;
    std::vector<lt::VideoCodecType> client_codecs_;
    bool run_as_win_service_;
    std::mutex mutex_;
    std::unique_ptr<ltlib::BlockingThread> thread_;
    std::atomic<bool> stoped_{true};
    void* process_handle_ = nullptr;
    void* thread_handle_ = nullptr;
    ltproto::Parser parser_;
    bool first_launch_ = true;
};

} // namespace svc

} // namespace lt