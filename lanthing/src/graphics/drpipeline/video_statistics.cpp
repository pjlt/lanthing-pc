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

#include "video_statistics.h"

#include <ltlib/times.h>

namespace lt {

void VideoStatistics::addHistory(std::deque<int64_t>& history) {
    int64_t now = ltlib::steady_now_us();
    const int64_t kOneSecond = 1'000'000;
    const int64_t kOneSecondBefore = now - kOneSecond;
    history.push_back(now);
    while (!history.empty()) {
        if (history.front() < kOneSecondBefore) {
            history.pop_front();
        }
        else {
            break;
        }
    }
}

void VideoStatistics::updateTime(Time& time_entry, int64_t duration) {
    constexpr size_t kMaxSize = 60;
    constexpr int64_t kOneMinute = 60'000'000;
    time_entry.history.push_back(duration);
    while (time_entry.history.size() > kMaxSize) {
        time_entry.history.pop_front();
    }
    int64_t sum = 0;
    for (auto value : time_entry.history) {
        sum += value;
    }
    time_entry.avg = sum / time_entry.history.size();
    int64_t now = ltlib::steady_now_us();
    if (time_entry.last_clear_time + kOneMinute < now) {
        time_entry.last_clear_time = now;
        time_entry.max = duration;
        time_entry.min = duration;
    }
    else {
        time_entry.max = std::max(time_entry.max, duration);
        time_entry.min = std::min(time_entry.min, duration);
    }
}

VideoStatistics::Stat VideoStatistics::getStat() {
    std::lock_guard lock{mutex_};
    Stat stat{};
    stat.encode_time = encode_time_;
    stat.render_video_time = render_video_time_;
    stat.render_widgets_time = render_widgets_time_;
    stat.present_time = present_time_;
    stat.net_delay = net_delay_;
    stat.decode_time = decode_time_;

    stat.render_video_fps = render_video_history_.size();
    stat.present_fps = present_history_.size();
    stat.encode_fps = encode_history_.size();
    stat.capture_fps = capture_history_.size();

    return stat;
}

void VideoStatistics::addRenderVideo() {
    std::lock_guard lock{mutex_};
    addHistory(render_video_history_);
}

void VideoStatistics::addPresent() {
    std::lock_guard lock{mutex_};
    addHistory(present_history_);
}

void VideoStatistics::addEncode() {
    std::lock_guard lock{mutex_};
    addHistory(encode_history_);
}

void VideoStatistics::updateEncodeTime(int64_t duration) {
    std::lock_guard lock{mutex_};
    updateTime(encode_time_, duration);
}

void VideoStatistics::updateRenderVideoTime(int64_t duration) {
    std::lock_guard lock{mutex_};
    updateTime(render_video_time_, duration);
}

void VideoStatistics::updateRenderWidgetsTime(int64_t duration) {
    std::lock_guard lock{mutex_};
    updateTime(render_widgets_time_, duration);
}

void VideoStatistics::updatePresentTime(int64_t duration) {
    std::lock_guard lock{mutex_};
    updateTime(present_time_, duration);
}

void VideoStatistics::updateNetDelay(int64_t duration) {
    std::lock_guard lock{mutex_};
    updateTime(net_delay_, duration);
}

void VideoStatistics::updateDecodeTime(int64_t duration) {
    std::lock_guard lock{mutex_};
    updateTime(decode_time_, duration);
}

void VideoStatistics::addVideoBW(int64_t) {}

void VideoStatistics::updateLossRate(const std::vector<float>&) {}

void VideoStatistics::addCapture(const std::vector<uint32_t>&) {
    std::lock_guard lock{mutex_};
    addHistory(capture_history_);
}

void VideoStatistics::updateBWE(const std::vector<uint32_t>&) {}

} // namespace lt