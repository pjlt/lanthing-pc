#pragma once
#include <ltlib/ltlib.h>
#include <string>

namespace ltlib
{

template <typename CharT>
std::basic_string<CharT> LT_API getProgramFullpath();

template <>
std::string LT_API getProgramFullpath<char>();

template <typename CharT>
std::basic_string<CharT> LT_API getProgramPath();

template <>
std::wstring LT_API getProgramFullpath<wchar_t>();

template <>
std::string LT_API getProgramPath<char>();

template <>
std::wstring LT_API getProgramPath<wchar_t>();

std::string LT_API getAppdataPath(bool is_service);

uint32_t LT_API getSessionIdByPid(uint32_t pid);

uint32_t LT_API getParentPid(uint32_t curr_pid);

bool LT_API isRunasLocalSystem();
bool LT_API isRunAsService();

int32_t LT_API getScreenWidth();
int32_t LT_API getScreenHeight();

bool LT_API setThreadDesktop();

struct LT_API DisplayOutputDesc
{
    DisplayOutputDesc() = delete;
    DisplayOutputDesc(uint32_t w, uint32_t h, uint32_t f)
        : width(w)
        , height(h)
        , frequency(f)
    {
    }
    int32_t width;
    int32_t height;
    int32_t frequency;
};
DisplayOutputDesc LT_API getDisplayOutputDesc();

} // namespace ltlib