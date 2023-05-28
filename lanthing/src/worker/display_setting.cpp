#include <Windows.h>
#include <set>
#include <g3log/g3log.hpp>
#include "display_setting.h"

namespace lt
{

namespace worker
{

DisplaySetting DisplaySettingNegotiator::negotiate(DisplaySetting client_display_setting)
{
    DEVMODE current_mode {};
    current_mode.dmSize = sizeof(DEVMODE);
    if (EnumDisplaySettingsW(nullptr, ENUM_CURRENT_SETTINGS, &current_mode) == 0) {
        LOG(WARNING) << "Enumerate current display settings failed";
        return {};
    }

    // 比较{width, height, refresh_rate ± 1}
    std::set<DisplaySetting, decltype(&DisplaySetting::compare_full_loose)> available_settings { &DisplaySetting::compare_full_loose };
    DEVMODE mode {};
    mode.dmSize = sizeof(DEVMODE);
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
    std::set<DisplaySetting, decltype(&DisplaySetting::compare_width_height)> avaiable_settings2 { available_settings.begin(), available_settings.end(), &DisplaySetting::compare_width_height };
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
    DisplaySetting result(current_mode.dmPelsWidth, current_mode.dmPelsHeight, current_mode.dmDisplayFrequency);
    return result;
}

bool DisplaySetting::compare_full_strict(const DisplaySetting& lhs, const DisplaySetting& rhs)
{
    if (lhs.width != rhs.width) {
        return lhs.width < rhs.width;
    }
    if (lhs.height != rhs.height) {
        return lhs.height < rhs.height;
    }
    return lhs.refrash_rate < rhs.refrash_rate;
}

bool DisplaySetting::compare_full_loose(const DisplaySetting& lhs, const DisplaySetting& rhs)
{
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

bool DisplaySetting::compare_width_height(const DisplaySetting& lhs, const DisplaySetting& rhs)
{
    if (lhs.width != rhs.width) {
        return lhs.width < rhs.width;
    }
    return lhs.height < rhs.height;
}

} // namespace worker

} // namespace lt