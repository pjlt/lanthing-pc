#pragma once
#include <cstdint>
#include <memory>
#include <mutex>
#include <array>
#include <optional>
#include <SDL.h>
#include <client/input/input_event.h>

namespace lt
{

namespace cli
{

constexpr uint8_t kMaxControllers = 4;

class SdlInput
{
public:
    struct Params
    {
        SDL_Window* window;
    };

public:
    static std::unique_ptr<SdlInput> create(const Params& params);
    void set_input_handler(const OnInputEvent& on_input_event);
    void rumble(uint16_t controller_number, uint16_t log_freq_motor, uint16_t high_freq_motor);
    void release_all_keys(); // TODO: 看看fl的这部分逻辑
    void on_mouse_leave();
    void on_focus_lost();
    void on_mouse_pos();
    void update_keyboard_grab(); //??
    void is_capture_active(); //???
    void is_system_key_capture_active(); //???
    void set_capture_active();
    void is_mouse_in_video_region(int32_t x, int32_t y, int32_t width, int32_t height);

    void handle_key_up_down(const SDL_KeyboardEvent& ev);
    void handle_mouse_button(const SDL_MouseButtonEvent& ev);
    void handle_mouse_move(const SDL_MouseMotionEvent& ev);
    void handle_mouse_wheel(const SDL_MouseWheelEvent& ev);
    void handle_controller_axis(const SDL_ControllerAxisEvent& ev);
    void handle_controller_button(const SDL_ControllerButtonEvent& ev);
    void handle_controller_added(const SDL_ControllerDeviceEvent& ev);
    void handle_controller_removed(const SDL_ControllerDeviceEvent& ev);
    void handle_joystick_added(const SDL_JoyDeviceEvent& ev);

private:
    SdlInput(const Params& params);
    void init();
    void on_input_event(const InputEvent& ev);

private:
    struct ControllerState
    {
        SDL_GameController* controller = nullptr;
        SDL_JoystickID joystick_id = -1;
        uint8_t index = std::numeric_limits<uint8_t>::max();
    };

private:
    // 0表示没按下，其他任意数字表示按下
    SDL_Window* window_;
    uint8_t keyboard_state_[512] = { 0 };
    std::mutex mutex_;
    std::function<void(const InputEvent&)> on_input_event_;
    std::array<std::optional<ControllerState>, kMaxControllers> controller_states_;
};

} // namespace cli

} // namespace lt