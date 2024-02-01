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
#include <array>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>

#include <SDL.h>

#include <inputs/capturer/input_event.h>

namespace lt {

namespace plat {

constexpr uint8_t kMaxControllers = 4;

class SdlInput {
public:
    struct Params {
        SDL_Window* window;
    };

public:
    static std::unique_ptr<SdlInput> create(const Params& params);
    void setInputHandler(const input::OnInputEvent& on_input_event);
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
    void onInputEvent(const input::InputEvent& ev);

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
    std::function<void(const input::InputEvent&)> on_input_event_;
    std::array<std::optional<ControllerState>, kMaxControllers> controller_states_;
};

} // namespace plat

} // namespace lt