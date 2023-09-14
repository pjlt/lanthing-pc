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

#include "video_capturer.h"

#include <g3log/g3log.hpp>

#include "dxgi_video_capturer.h"

namespace lt {

std::unique_ptr<VideoCapturer> VideoCapturer::create(const Params& params) {
    if (params.backend != Backend::Dxgi) {
        LOG(FATAL) << "Only support dxgi video capturer!";
        return nullptr;
    }
    if (params.on_frame == nullptr) {
        LOG(FATAL) << "Create video capturer without callback!";
        return nullptr;
    }
    auto capturer = std::make_unique<DxgiVideoCapturer>();
    capturer->on_frame_ = params.on_frame;
    if (!capturer->init()) {
        return nullptr;
    }
    return capturer;
}

VideoCapturer::VideoCapturer()
    : stop_promise_{std::make_unique<std::promise<void>>()} {}

VideoCapturer::~VideoCapturer() {
    stop();
}

void VideoCapturer::start() {
    thread_ = ltlib::BlockingThread::create(
        "video_capture",
        [this](const std::function<void()>& i_am_alive, void*) { mainLoop(i_am_alive); }, nullptr);
}

bool VideoCapturer::init() {
    if (!preInit()) {
        return false;
    }
    return true;
}

void VideoCapturer::mainLoop(const std::function<void()>& i_am_alive) {
    stoped_ = false;
    LOG(INFO) << "Video capturer started";
    while (!stoped_) {
        i_am_alive();
        auto frame = captureOneFrame();
        if (frame) {
            // TODO: 设置其他frame的其他值
            frame->set_picture_id(frame_no_++);
            on_frame_(frame);
        }
        waitForVblank();
    }
    stop_promise_->set_value();
}

void VideoCapturer::stop() {
    // 即便stoped_是原子也不应该这么做，但这个程序似乎不会出现一个线程正在析构VideoCapture，另一个线程主动调stop()
    if (!stoped_) {
        stoped_ = true;
        stop_promise_->get_future().get();
    }
}

} // namespace lt