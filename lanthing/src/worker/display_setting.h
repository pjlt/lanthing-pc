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

namespace lt {

namespace worker {

struct DisplaySetting {
    DisplaySetting() = default;
    DisplaySetting(uint32_t w, uint32_t h, uint32_t r)
        : width(w)
        , height(h)
        , refrash_rate(r) {}
    bool operator==(const DisplaySetting& other) const {
        return width == other.width && height == other.height && refrash_rate == other.refrash_rate;
    }
    bool operator!=(const DisplaySetting& other) const { return !(*this == other); }
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t refrash_rate = 0;
    static bool compareFullStrict(const DisplaySetting& lhs, const DisplaySetting& rhs);
    static bool compareFullLoose(const DisplaySetting& lhs, const DisplaySetting& rhs);
    static bool compareWidthHeight(const DisplaySetting& lhs, const DisplaySetting& rhs);
};

class DisplaySettingNegotiator {
public:
    struct Result {
        DisplaySetting client;
        DisplaySetting service;
        DisplaySetting negotiated;
    };

public:
    DisplaySettingNegotiator() = delete;
    ~DisplaySettingNegotiator() = delete;
    static Result negotiate(DisplaySetting client_display_setting);
};

} // namespace worker

} // namespace lt