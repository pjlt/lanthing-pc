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
#include <atomic>
#include <functional>
#include <future>
#include <memory>

#include <ltproto/peer2peer/capture_video_frame.pb.h>

#include <ltlib/threads.h>

namespace lt {

class VideoCapturer {
public:
    enum class Backend {
        Dxgi,
    };
    using OnFrame = std::function<void(std::shared_ptr<ltproto::peer2peer::CaptureVideoFrame>)>;

    struct Params {
        Backend backend;
        OnFrame on_frame;
    };

public:
    static std::unique_ptr<VideoCapturer> create(const Params& params);
    virtual ~VideoCapturer();
    void start();
    void stop();
    virtual void releaseFrame(const std::string& name) = 0;
    virtual Backend backend() const = 0;
    virtual int64_t luid() { return -1; }

protected:
    VideoCapturer();
    virtual bool preInit() = 0;
    virtual std::shared_ptr<ltproto::peer2peer::CaptureVideoFrame> captureOneFrame() = 0;
    // virtual void done_with_frame() = 0;
    virtual void waitForVblank() = 0;

private:
    bool init();
    void mainLoop(const std::function<void()>& i_am_alive);

private:
    OnFrame on_frame_;
    std::unique_ptr<ltlib::BlockingThread> thread_;
    // std::mutex mutex_;
    // std::condition_variable cv_;
    uint64_t frame_no_ = 0;
    std::atomic<bool> stoped_{true};
    std::unique_ptr<std::promise<void>> stop_promise_;
};

} // namespace lt