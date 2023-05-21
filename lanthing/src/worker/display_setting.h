#pragma once
#include <cstdint>

namespace lt
{

namespace worker
{

struct DisplaySetting
{
    DisplaySetting() = default;
    DisplaySetting(uint32_t w, uint32_t h, uint32_t r)
        : width(w)
        , height(h)
        , refrash_rate(r)
    {
    }
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t refrash_rate = 0;
    static bool compare_full_strict(const DisplaySetting& lhs, const DisplaySetting& rhs);
    static bool compare_full_loose(const DisplaySetting& lhs, const DisplaySetting& rhs);
    static bool compare_width_height(const DisplaySetting& lhs, const DisplaySetting& rhs);
};

class DisplaySettingNegotiator
{
public:
    DisplaySettingNegotiator() = delete;
    ~DisplaySettingNegotiator() = delete;
    static DisplaySetting negotiate(DisplaySetting client_display_setting);
};

} // namespace worker

} // namespace lt