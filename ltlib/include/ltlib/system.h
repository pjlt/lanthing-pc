#pragma once
#include <ltlib/ltlib.h>
#include <string>

namespace ltlib
{

template <typename CharT>
std::basic_string<CharT> LT_API get_program_fullpath();

template <>
std::string LT_API get_program_fullpath<char>();

template <typename CharT>
std::basic_string<CharT> LT_API get_program_path();

template <>
std::wstring LT_API get_program_fullpath<wchar_t>();

template <>
std::string LT_API get_program_path<char>();

template <>
std::wstring LT_API get_program_path<wchar_t>();

std::string LT_API get_appdata_path(bool is_service);

uint32_t LT_API get_session_id_by_pid(uint32_t pid);

uint32_t LT_API get_parent_pid(uint32_t curr_pid);

bool LT_API is_run_as_local_system();
bool LT_API is_run_as_service();

int32_t LT_API get_screen_width();
int32_t LT_API get_screen_height();

bool LT_API set_thread_desktop();

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
DisplayOutputDesc LT_API get_display_output_desc();

} // namespace ltlib