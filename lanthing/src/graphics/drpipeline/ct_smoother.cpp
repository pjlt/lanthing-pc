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

#include "ct_smoother.h"

namespace lt {

// 先不做任何平滑
void CTSmoother::push(Frame frame) {
    std::lock_guard<std::mutex> lock(buf_mtx_);
    frames_.clear();
    frames_.push_back(frame);
}

void CTSmoother::pop() {
    std::lock_guard<std::mutex> lock(buf_mtx_);
    if (frames_.empty()) {
        return;
    }
    frames_.pop_front();
}

size_t CTSmoother::size() const {
    std::lock_guard<std::mutex> lock(buf_mtx_);
    return frames_.size();
}

void CTSmoother::clear() {
    std::lock_guard<std::mutex> lock(buf_mtx_);
    frames_.clear();
}

std::optional<CTSmoother::Frame> CTSmoother::get(int64_t at_time) {
    (void)at_time;
    std::lock_guard<std::mutex> lock(buf_mtx_);
    if (frames_.empty()) {
        return {};
    }
    return frames_.front();
}
} // namespace lt
