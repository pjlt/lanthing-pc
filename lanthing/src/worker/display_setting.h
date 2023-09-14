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
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t refrash_rate = 0;
    static bool compareFullStrict(const DisplaySetting& lhs, const DisplaySetting& rhs);
    static bool compareFullLoose(const DisplaySetting& lhs, const DisplaySetting& rhs);
    static bool compareWidthHeight(const DisplaySetting& lhs, const DisplaySetting& rhs);
};

class DisplaySettingNegotiator {
public:
    DisplaySettingNegotiator() = delete;
    ~DisplaySettingNegotiator() = delete;
    static DisplaySetting negotiate(DisplaySetting client_display_setting);
};

} // namespace worker

} // namespace lt