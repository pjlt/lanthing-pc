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

#include <Windows.h>

#include "display_setting.h"

#include <g3log/g3log.hpp>
#include <set>

namespace lt {

namespace worker {

DisplaySetting DisplaySettingNegotiator::negotiate(DisplaySetting client_display_setting) {
    DEVMODEW current_mode{};
    current_mode.dmSize = sizeof(DEVMODEW);
    if (EnumDisplaySettingsW(nullptr, ENUM_CURRENT_SETTINGS, &current_mode) == 0) {
        LOG(WARNING) << "Enumerate current display settings failed";
        return {};
    }

    // 比较{width, height, refresh_rate ± 1}
    std::set<DisplaySetting, decltype(&DisplaySetting::compareFullLoose)> available_settings{
        &DisplaySetting::compareFullLoose};
    DEVMODEW mode{};
    mode.dmSize = sizeof(DEVMODEW);
    DWORD mode_num = 0;
    while (EnumDisplaySettingsW(nullptr, mode_num, &mode) != 0) {
        mode_num += 1;
        DisplaySetting setting(mode.dmPelsWidth, mode.dmPelsHeight, mode.dmDisplayFrequency);
        available_settings.insert(setting);
    }
    const auto iter = available_settings.find(client_display_setting);
    if (iter != available_settings.cend()) {
        return *iter;
    }

    // 比较{width, height}
    std::set<DisplaySetting, decltype(&DisplaySetting::compareWidthHeight)> avaiable_settings2{
        available_settings.begin(), available_settings.end(), &DisplaySetting::compareWidthHeight};
    auto iter2 = avaiable_settings2.find(client_display_setting);
    if (iter2 != avaiable_settings2.end()) {
        DisplaySetting result = *iter2;
        result.refrash_rate = 0; // 置0表示刷新率协商失败.
        return result;
    }

    // 找到 分辨率最接近client_display_setting && 分辨率 < client_display_setting 的配置.
    auto iter3 = avaiable_settings2.lower_bound(client_display_setting);
    if (iter3 != avaiable_settings2.end()) {
        iter3--;
        if (iter3 != avaiable_settings2.end()) {
            DisplaySetting result = *iter3;
            result.refrash_rate = 0;
            return result;
        }
    }
    // 找不到，直接返回host当前的DisplaySetting
    DisplaySetting result(current_mode.dmPelsWidth, current_mode.dmPelsHeight,
                          current_mode.dmDisplayFrequency);
    return result;
}

bool DisplaySetting::compareFullStrict(const DisplaySetting& lhs, const DisplaySetting& rhs) {
    if (lhs.width != rhs.width) {
        return lhs.width < rhs.width;
    }
    if (lhs.height != rhs.height) {
        return lhs.height < rhs.height;
    }
    return lhs.refrash_rate < rhs.refrash_rate;
}

bool DisplaySetting::compareFullLoose(const DisplaySetting& lhs, const DisplaySetting& rhs) {
    if (lhs.width != rhs.width) {
        return lhs.width < rhs.width;
    }
    if (lhs.height != rhs.height) {
        return lhs.height < rhs.height;
    }
    if (rhs.refrash_rate < 2) {
        return false;
    }
    return (rhs.refrash_rate > lhs.refrash_rate) && (rhs.refrash_rate - lhs.refrash_rate > 2);
}

bool DisplaySetting::compareWidthHeight(const DisplaySetting& lhs, const DisplaySetting& rhs) {
    if (lhs.width != rhs.width) {
        return lhs.width < rhs.width;
    }
    return lhs.height < rhs.height;
}

} // namespace worker

} // namespace lt