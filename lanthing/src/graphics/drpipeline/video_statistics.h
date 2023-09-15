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
#include <deque>
#include <memory>
#include <mutex>
#include <vector>

namespace lt {

class VideoStatistics {
public:
    struct Time {
        std::deque<int64_t> history;
        int64_t last_clear_time = 0;
        int64_t max;
        int64_t min;
        int64_t avg;
    };
    struct Stat {
        Time encode_time;
        Time render_video_time;
        Time render_widgets_time;
        Time present_time;
        Time net_delay;
        Time decode_time;
        int64_t render_video_fps;
        int64_t present_fps;
        int64_t encode_fps;
        int64_t capture_fps;
    };

public:
    VideoStatistics() = default;
    Stat getStat();
    void addRenderVideo();
    void addPresent();
    void addEncode();
    void updateEncodeTime(int64_t duration); // 跟视频数据一并从host传输过来
    void updateRenderVideoTime(int64_t duration);
    void updateRenderWidgetsTime(int64_t duration);
    void updatePresentTime(int64_t duration);
    void updateNetDelay(int64_t duration);
    void updateDecodeTime(int64_t duration);
    void addVideoBW(int64_t bytes); // 特殊处理

    // 独立消息
    void updateLossRate(const std::vector<float>& loss);
    void addCapture(const std::vector<uint32_t>& fps);
    void updateBWE(const std::vector<uint32_t>& bps);

private:
    static void addHistory(std::deque<int64_t>& history);
    static void updateTime(Time& time_entry, int64_t duration);

private:
    std::mutex mutex_;
    std::deque<int64_t> render_video_history_;
    std::deque<int64_t> present_history_;
    std::deque<int64_t> encode_history_;
    std::deque<int64_t> capture_history_;
    Time encode_time_;
    Time render_video_time_;
    Time render_widgets_time_;
    Time present_time_;
    Time net_delay_;
    Time decode_time_;
};

} // namespace lt