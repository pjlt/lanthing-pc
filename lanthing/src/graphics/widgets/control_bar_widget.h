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
#include <string>

namespace lt {

class ControlBarWidget {
public:
    struct Params {
        uint32_t video_width;
        uint32_t video_height;
        std::function<void()> toggle_fullscreen;
        std::function<void(uint32_t bps)> set_bitrate; // 0代表自动
        std::function<void()> exit;
        std::function<void(bool)> show_stat;
        std::function<void()> switch_monitor;
        std::function<void()> stretch;
    };

public:
    ControlBarWidget(const Params& params);
    void render();
    void update();

private:
    uint32_t video_width_;
    uint32_t video_height_;
    std::function<void()> toggle_fullscreen_;
    std::function<void(uint32_t bps)> set_bitrate_; // 0代表自动
    std::function<void()> exit_;
    std::function<void(bool)> on_show_stat_;
    std::function<void()> switch_monitor_;
    std::function<void()> stretch_;
    bool collapse_ = true;
    std::string fullscreen_text_;
    bool fullscreen_ = false;
    int radio_ = 0;
    int manual_bitrate_ = 2;
    bool show_stat_ = false;
    std::string stat_text_;
    bool first_time_ = true;
};

} // namespace lt