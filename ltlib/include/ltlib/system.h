#pragma once
#include <string>

namespace ltlib
{

template <typename CharT>
std::basic_string<CharT> get_program_fullpath();

template <typename CharT>
std::basic_string<CharT> get_program_path();

std::string get_appdata_path(bool is_service);

uint32_t get_session_id_by_pid(uint32_t pid);

uint32_t get_parent_pid(uint32_t curr_pid);

bool is_run_as_local_system();
bool is_run_as_service();

int32_t get_screen_width();
int32_t get_screen_height();

bool set_thread_desktop();

struct DisplayOutputDesc
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
DisplayOutputDesc get_display_output_desc();

} // namespace ltlib