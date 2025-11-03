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

#include <ltlib/logging.h>

#include <ltlib/times.h>

#include "dxgi_video_capturer.h"
#include "nvfbc_video_capturer.h"

namespace lt {

namespace video {

std::unique_ptr<Capturer> Capturer::create(Backend backend, ltlib::Monitor monitor) {
    switch (backend) {
    case lt::video::Capturer::Backend::Dxgi:
    {
        auto capturer = std::make_unique<DxgiVideoCapturer>(monitor);
        if (!capturer->init()) {
            return nullptr;
        }
        return capturer;
    }
    case lt::video::Capturer::Backend::Nvfbc:
    {
        auto capturer = std::make_unique<NvFBCVideoCapturer>(monitor);
        if (!capturer->init()) {
            return nullptr;
        }
        return capturer;
    }
    default:
        LOG(ERR) << "Unknown Capturer backend " << (int)backend;
        return nullptr;
    }
}

Capturer::Capturer() = default;

Capturer::~Capturer() = default;

} // namespace video

} // namespace lt