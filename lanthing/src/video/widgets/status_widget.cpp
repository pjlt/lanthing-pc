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

#include "status_widget.h"

#include <cstdio>

#include <SDL.h>
#include <SDL_syswm.h>
#include <imgui.h>

// NOTE: 暂时写死D3D11+SDL2

namespace lt {

namespace video {

StatusWidget::StatusWidget(uint32_t video_width, uint32_t video_height, int64_t color)
    : video_width_{video_width}
    , video_height_{video_height} {
    resize_ = true;
    if (color < 0) {
        red_ = .5f;
        green_ = .5f;
        blue_ = .5f;
    }
    else {
        uint32_t ucolor = static_cast<uint32_t>(color);
        red_ = ((ucolor & 0xff000000) >> 24) / 255.f;
        green_ = ((ucolor & 0x00ff0000) >> 16) / 255.f;
        blue_ = ((ucolor & 0x0000ff00) >> 8) / 255.f;
    }
}

StatusWidget::~StatusWidget() {}

void StatusWidget::setTaskBarPos(uint32_t direction, uint32_t left, uint32_t right, uint32_t top,
                                 uint32_t bottom) {
    if (direction == 2) {
        right_margin_ = right - left;
    }
    if (direction == 3) {
        bottom_margin_ = bottom - top;
    }
}

void StatusWidget::render() {
    char buff[64] = {0};
    // const char* kFormat = "RTT:%u  FPS:%u  LOSS:%2.1f%%";
    sprintf(buff, "RTT:%u  FPS:%u  LOSS:%2.1f  ", rtt_ms_, fps_, loss_ * 100.f);
    auto kTextSize = ImGui::CalcTextSize(buff);
    auto& io = ImGui::GetIO();
    display_width_ = static_cast<uint32_t>(io.DisplaySize.x);
    display_height_ = static_cast<uint32_t>(io.DisplaySize.y);
    float x = static_cast<float>(display_width_ - kTextSize.x - right_margin_);
    float y = static_cast<float>(display_height_ - kTextSize.y - bottom_margin_);
    ImGui::SetNextWindowPos(ImVec2{x, y});
    ImGui::SetNextWindowSize(ImVec2{kTextSize.x + 10, kTextSize.y});
    ImGui::Begin("status", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs |
                     ImGuiWindowFlags_NoBackground);
    ImGui::TextColored(ImVec4{red_, green_, blue_, 1.f}, "%s", buff);
    ImGui::End();
}

void StatusWidget::update(uint32_t rtt_ms, uint32_t fps, float loss) {
    rtt_ms_ = rtt_ms;
    fps_ = fps;
    loss_ = loss;
}

void StatusWidget::resize() {
    resize_ = true;
}

} // namespace video

} // namespace lt