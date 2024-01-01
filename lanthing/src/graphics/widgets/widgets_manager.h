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
#include <memory>
#include <string>
#include <vector>

#include <graphics/drpipeline/video_statistics.h>

namespace lt {

class StatusWidget;
class StatisticsWidget;
class ControlBarWidget;

struct Statistics {
    std::string name;
    float value;
};

class WidgetsManager {
public:
    struct Params {
        void* dev;
        void* ctx;
        void* window;
        uint32_t video_width;
        uint32_t video_height;
        // std::function<void(bool)> toggle_fullscreen;
        // std::function<void()> exit;
        std::function<void(uint32_t bps)> set_bitrate; // 0代表自动
        std::function<void()> switch_monitor;
    };

public:
    static std::unique_ptr<WidgetsManager> create(const Params& params);
    ~WidgetsManager();
    void render();
    void reset();
    void enableStatus();
    void disableStatus();
    void enableStatistics();
    void disableStatistics();
    void setTaskBarPos(uint32_t direction, uint32_t left, uint32_t right, uint32_t top,
                       uint32_t bottom);
    void updateStatus(uint32_t rtt_ms, uint32_t fps, float loss);
    void updateStatistics(const VideoStatistics::Stat& statistics);

private:
    WidgetsManager(const Params& params);
    void initImgui();
    void uninitImgui();
    void imguiImplInit();
    void imguiImplShutdown();
    void imguiImplNewFrame();
    void imguiImplRender();

private:
    void* dev_;
    void* ctx_;
    void* window_ = nullptr;
    bool show_status_ = true;
    bool show_statistics_ = false;
    uint32_t video_width_;
    uint32_t video_height_;
    // bool show_control_bar_ = true;
    std::function<void(uint32_t bps)> set_bitrate_;
    std::function<void()> switch_monitor_;
    std::shared_ptr<StatusWidget> status_;
    std::shared_ptr<StatisticsWidget> statistics_;
    std::shared_ptr<ControlBarWidget> control_bar_;
};

} // namespace lt