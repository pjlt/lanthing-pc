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

#include "control_bar_widget.h"

#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui.h>

namespace lt {

ControlBarWidget::ControlBarWidget(uint32_t video_width, uint32_t video_height,
                                   uint32_t display_width, uint32_t display_height)
    : video_width_{video_width}
    , video_height_{video_height}
    , display_width_{display_width}
    , display_height_{display_height} {}

void ControlBarWidget::render() {
    constexpr float width = 20.f;
    constexpr float height = 10.f;
    ImVec2 a{-width / 2, 0.f};
    ImVec2 b{width / 2, 0.f};
    ImVec2 c{0.f, height};

    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos({(io.DisplaySize.x - width) / 2, 0}, ImGuiCond_Always);
    ImGui::SetNextWindowSize({width, height});
    ImGui::Begin("control_bar", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs |
                     ImGuiWindowFlags_NoBackground);
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    ImVec2 middle{io.DisplaySize.x / 2, 0.f};
    ImVec4 color{.5f, .5f, .5f, .5f};
    draw_list->AddTriangleFilled(middle + a, middle + b, middle + c,
                                 ImGui::ColorConvertFloat4ToU32(color));
    ImGui::End();
}

void ControlBarWidget::update() {}

} // namespace lt