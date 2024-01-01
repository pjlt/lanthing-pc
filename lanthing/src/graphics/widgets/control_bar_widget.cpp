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

ControlBarWidget::ControlBarWidget(const Params& params)
    : video_width_{params.video_width}
    , video_height_{params.video_height}
    , toggle_fullscreen_{params.toggle_fullscreen}
    , set_bitrate_{params.set_bitrate}
    , exit_{params.exit}
    , on_show_stat_{params.show_stat}
    , switch_monitor_{params.switch_monitor}
    , stretch_{params.stretch} {
    // fullscreen_text_ = u8"全屏";
    // stat_text_ = u8"显示统计";
    fullscreen_text_ = "Fullscreen";
    stat_text_ = "Show Stat";
}

void ControlBarWidget::render() {
    if (first_time_) {
        first_time_ = false;
        auto& io = ImGui::GetIO();
        ImGui::SetNextWindowPos(ImVec2{(io.DisplaySize.x - 24.f) / 2, 0.f});
        ImGui::SetNextWindowCollapsed(true);
    }
    if (collapse_) {
        ImGui::SetNextWindowSize(ImVec2{24.f, 24.f});
    }
    else {
        ImGui::SetNextWindowSize(ImVec2{320.f, 220.f});
    }

    ImGui::Begin("Tool", nullptr,
                 ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoNavInputs |
                     ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoResize);
    if (ImGui::IsWindowCollapsed()) {
        collapse_ = true;
    }
    else {
        // FIXME: 应该读取当前窗口模式，而不是记录是否全屏，因为还有“快捷键切换”，记录不到这里
        collapse_ = false;
        if (ImGui::Button(fullscreen_text_.c_str())) {
            if (fullscreen_) {
                fullscreen_ = false;
                // fullscreen_text_ = u8"全屏";
                fullscreen_text_ = "Fullscreen";
                toggle_fullscreen_();
            }
            else {
                fullscreen_ = true;
                // fullscreen_text_ = u8"窗口化";
                fullscreen_text_ = "Windowed";
                toggle_fullscreen_();
            }
        }
        if (ImGui::Button(stat_text_.c_str())) {
            if (show_stat_) {
                // stat_text_ = u8"显示统计";
                stat_text_ = "Show stat";
            }
            else {
                // stat_text_ = u8"关闭统计";
                stat_text_ = "Hide stat";
            }
            show_stat_ = !show_stat_;
            on_show_stat_(show_stat_);
        }
        ImGui::Text("Bitrate:");
        // if (ImGui::RadioButton(u8"自动", &radio_, 0)) {
        if (ImGui::RadioButton("Auto", &radio_, 0)) {
            set_bitrate_(0);
        }
        // if (ImGui::RadioButton(u8"手动", &radio_, 1)) {
        if (ImGui::RadioButton("Manual", &radio_, 1)) {
            //
        }
        if (radio_ == 1) {
            ImGui::SameLine();
            ImGui::PushItemWidth(ImGui::GetWindowWidth() * 0.6f);
            if (ImGui::SliderInt("Mbps", &manual_bitrate_, 2, 100, "%d")) {
                set_bitrate_(static_cast<uint32_t>(manual_bitrate_) * 1024 * 1024);
            }
            ImGui::PopItemWidth();
        }
        if (ImGui::Button("Switch Screen")) {
            switch_monitor_();
        }
        if (ImGui::Button("Stretch/Origin")) {
            stretch_();
        }
        // if (ImGui::Button(u8"退出")) {
        if (ImGui::Button("Quit")) {
            exit_();
        }
    }

    ImGui::End();
}

void ControlBarWidget::update() {}

} // namespace lt