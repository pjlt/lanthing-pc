#pragma once
#include <array>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>

#include <SDL.h>

#include <inputs/capturer/input_event.h>

namespace lt {

constexpr uint8_t kMaxControllers = 4;

class SdlInput {
public:
    struct Params {
        SDL_Window* window;
    };

public:
    static std::unique_ptr<SdlInput> create(const Params& params);
    void setInputHandler(const OnInputEvent& on_input_event);
    void rumble(uint16_t controller_number, uint16_t log_freq_motor, uint16_t high_freq_motor);

    void handleKeyUpDown(const SDL_KeyboardEvent& ev);
    void handleMouseButton(const SDL_MouseButtonEvent& ev);
    void handleMouseMove(const SDL_MouseMotionEvent& ev);
    void handleMouseWheel(const SDL_MouseWheelEvent& ev);
    void handleControllerAxis(const SDL_ControllerAxisEvent& ev);
    void handleControllerButton(const SDL_ControllerButtonEvent& ev);
    void handleControllerAdded(const SDL_ControllerDeviceEvent& ev);
    void handleControllerRemoved(const SDL_ControllerDeviceEvent& ev);
    void handleJoystickAdded(const SDL_JoyDeviceEvent& ev);

private:
    SdlInput(const Params& params);
    void init();
    void onInputEvent(const InputEvent& ev);

private:
    struct ControllerState {
        SDL_GameController* controller = nullptr;
        SDL_JoystickID joystick_id = -1;
        uint8_t index = std::numeric_limits<uint8_t>::max();
    };

private:
    // 0表示没按下，其他任意数字表示按下
    SDL_Window* window_;
    uint8_t keyboard_state_[512] = {0};
    std::mutex mutex_;
    std::function<void(const InputEvent&)> on_input_event_;
    std::array<std::optional<ControllerState>, kMaxControllers> controller_states_;
};

} // namespace lt