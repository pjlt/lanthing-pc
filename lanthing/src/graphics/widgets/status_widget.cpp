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

#include <d3d11.h>

#include <SDL.h>
#include <SDL_syswm.h>
#include <imgui.h>

// NOTE: 暂时写死D3D11+SDL2

namespace lt {

StatusWidget::StatusWidget(uint32_t video_width, uint32_t video_height, uint32_t display_width,
                           uint32_t display_height)
    : video_width_{video_width}
    , video_height_{video_height}
    , display_width_{display_width}
    , display_height_{display_height} {}

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
    const uint32_t kAssumeWidth = 250;
    const uint32_t kAssumeHeight = 50;
    float x = static_cast<float>(video_width_ - kAssumeWidth - right_margin_);
    x = x * display_width_ / video_width_;
    float y = static_cast<float>(video_height_ - kAssumeHeight - bottom_margin_);
    y = y * display_height_ / video_height_;
    ImGui::SetNextWindowPos(ImVec2{x, y});
    ImGui::Begin("status", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs |
                     ImGuiWindowFlags_NoBackground);
    ImGui::TextColored(ImVec4{.5f, .5f, .5f, 1.f}, "RTT:%u  FPS:%u  LOSS:%.1f%% ", rtt_ms_, fps_,
                       loss_);
    ImGui::End();
}

void StatusWidget::update(uint32_t rtt_ms, uint32_t fps, float loss) {
    rtt_ms_ = rtt_ms;
    fps_ = fps;
    loss_ = loss;
}

} // namespace lt