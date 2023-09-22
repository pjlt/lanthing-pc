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

#include "statistics_widget.h"

#include <sstream>

#include <imgui.h>

namespace {

void plotLines(const std::string& name, const lt::VideoStatistics::History& history) {
    std::vector<float> values;
    values.resize(history.history.size());
    for (size_t i = 0; i < history.history.size(); i++) {
        values[i] = (float)history.history[i];
    }
    char buffer[128];
    const float max = std::min((float)history.max, 99999.f);
    sprintf(buffer, "%s min:%.0f max:%.0f avg:%.0f", name.c_str(), history.min, max, history.avg);
    ImGui::PlotLines(buffer, values.data(), static_cast<int>(values.size()), 0, "",
                     static_cast<float>(history.min), max, ImVec2{300.f, 0});
}

} // namespace

namespace lt {

StatisticsWidget::StatisticsWidget(uint32_t video_width, uint32_t video_height)
    : video_width_{video_width}
    , video_height_{video_height} {}

void StatisticsWidget::render() {
    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - 600, 40), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.7f);
    const char* format = R"(capture_fps: %d
encode_fps: %d
render_fps: %d
present_fps: %d
)";
    ImGui::Begin("statistics", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs |
                     ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::Text(format, stat_.capture_fps, stat_.encode_fps, stat_.render_video_fps,
                stat_.present_fps);
    plotLines("enc", stat_.encode_time);
    plotLines("ren", stat_.render_video_time);
    plotLines("wgt", stat_.render_widgets_time);
    plotLines("prs", stat_.present_time);
    plotLines("net", stat_.net_delay);
    plotLines("dec", stat_.decode_time);
    plotLines("bwe", stat_.bwe);
    plotLines("vbw", stat_.video_bw);
    plotLines("los", stat_.loss_rate);
    ImGui::End();
}

void StatisticsWidget::update(const VideoStatistics::Stat& statistics) {
    stat_ = statistics;
}

} // namespace lt