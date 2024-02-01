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
#include <future>
#include <memory>
#include <string>

#include <inputs/capturer/input_event.h>

extern "C" {

struct SDL_Window;

} // extern "C"

namespace lt {

class PcSdl {
public:
    struct Params {
        std::function<void()> on_reset;
        std::function<void()> on_exit;
        bool windowed_fullscreen = true;
        bool absolute_mouse = true;
        bool hide_window = false;
    };

public:
    static std::unique_ptr<PcSdl> create(const Params& params);
    virtual ~PcSdl(){};
    // virtual void set_negotiated_params(uint32_t width, uint32_t height) = 0;
    virtual SDL_Window* window() = 0;

    virtual void setInputHandler(const input::OnInputEvent&) = 0;

    virtual void toggleFullscreen() = 0;

    virtual void setTitle(const std::string& title) = 0;

    virtual void stop() = 0;

    virtual void switchMouseMode(bool absolute) = 0;

    virtual void setCursorInfo(int32_t cursor_id, bool visible) = 0;

protected:
    PcSdl() = default;
};

} // namespace lt