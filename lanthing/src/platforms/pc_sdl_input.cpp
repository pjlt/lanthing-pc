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

#include "pc_sdl_input.h"

#include <g3log/g3log.hpp>

namespace {

constexpr size_t kGUIDLength = 64;

} // namespace

namespace lt {

std::unique_ptr<SdlInput> SdlInput::create(const Params& params) {
    std::unique_ptr<SdlInput> input{new SdlInput{params}};
    input->init();
    return input;
}

SdlInput::SdlInput(const Params& params)
    : window_{params.window} {
    // TODO: 初始化
}

void SdlInput::init() {
    // TODO: 加载通过某种方式嵌入二进制文件的gamemapping文件
    // NOTE: 这里只有mapping_cout > 0这个分支条件是正常，其他都是为错误！
    std::string mapping_contents;
    if (!mapping_contents.empty()) {
        int mapping_count = SDL_GameControllerAddMappingsFromRW(
            SDL_RWFromConstMem(mapping_contents.c_str(), static_cast<int>(mapping_contents.size())),
            true);
        if (mapping_count > 0) {
            LOGF(INFO, "Successfully load %d controller mappings", mapping_count);
        }
        else if (mapping_count == 0) {
            LOG(WARNING) << "No controller mappings loaded";
        }
        else {
            LOG(WARNING) << "Map controller data failed";
        }
    }
    else {
        LOG(WARNING) << "No controller mappings data found";
    }
}

void SdlInput::setInputHandler(const OnInputEvent& on_input_event) {
    std::lock_guard lock{mutex_};
    on_input_event_ = on_input_event;
}

void SdlInput::handleKeyUpDown(const SDL_KeyboardEvent& ev) {
    if (ev.repeat) {
        return;
    }
    // 这个范围里并不是每一个数值都有对应的SDL Scancode，进一步过滤的逻辑交由业务代码去做！
    if (ev.keysym.scancode <= SDL_SCANCODE_UNKNOWN || ev.keysym.scancode >= SDL_NUM_SCANCODES) {
        return;
    }
    onInputEvent(KeyboardEvent{static_cast<uint16_t>(ev.keysym.scancode), ev.type == SDL_KEYDOWN});
}

void SdlInput::handleMouseButton(const SDL_MouseButtonEvent& ev) {
    // SdlInput属于platform层，只负责把窗口内所有鼠标button事件回调到业务层的Input
    // 渲染的视频可能只是这个窗口的一部分，判断点击是否在窗口内的逻辑，由业务层去做
    if (ev.which == SDL_TOUCH_MOUSEID) {
        return;
    }
    MouseButtonEvent::Button btn;
    switch (ev.button) {
    case SDL_BUTTON_LEFT:
        btn = MouseButtonEvent::Button::Left;
        break;
    case SDL_BUTTON_MIDDLE:
        btn = MouseButtonEvent::Button::Mid;
        break;
    case SDL_BUTTON_RIGHT:
        btn = MouseButtonEvent::Button::Right;
        break;
    case SDL_BUTTON_X1:
        btn = MouseButtonEvent::Button::X1;
        break;
    case SDL_BUTTON_X2:
        btn = MouseButtonEvent::Button::X2;
        break;
    default:
        // SDL会不会出bug？
        return;
    }
    int width;
    int height;
    SDL_GetWindowSize(window_, &width, &height);
    onInputEvent(MouseButtonEvent{btn, ev.state == SDL_PRESSED, ev.x, ev.y,
                                  static_cast<uint32_t>(width), static_cast<uint32_t>(height)});
}

void SdlInput::handleMouseMove(const SDL_MouseMotionEvent& ev) {
    if (ev.which == SDL_TOUCH_MOUSEID) {
        return;
    }
    int width;
    int height;
    SDL_GetWindowSize(window_, &width, &height);
    if (width <= 0 || height <= 0) {
        LOG(WARNING) << "Get window width/height failed";
        return;
    }
    onInputEvent(MouseMoveEvent{ev.x, ev.y, ev.xrel, ev.yrel, static_cast<uint32_t>(width),
                                static_cast<uint32_t>(height)});
}

void SdlInput::handleMouseWheel(const SDL_MouseWheelEvent& ev) {
    if (ev.which == SDL_TOUCH_MOUSEID) {
        return;
    }
    onInputEvent(MouseWheelEvent{ev.y * 120});
}

void SdlInput::handleControllerAxis(const SDL_ControllerAxisEvent& ev) {
    uint8_t index;
    for (index = 0; index < kMaxControllers; index++) {
        if (controller_states_[index].has_value() &&
            controller_states_[index]->joystick_id == ev.which) {
            break;
        }
    }
    if (index >= kMaxControllers) {
        return;
    }
    int16_t value;
    ControllerAxisEvent::AxisType axis_type;
    switch (ev.axis) {
    case SDL_CONTROLLER_AXIS_LEFTX:
        axis_type = ControllerAxisEvent::AxisType::LeftThumbX;
        value = ev.value;
        break;
    case SDL_CONTROLLER_AXIS_LEFTY:
        axis_type = ControllerAxisEvent::AxisType::LeftThumbY;
        value = ev.value;
        break;
    case SDL_CONTROLLER_AXIS_RIGHTX:
        axis_type = ControllerAxisEvent::AxisType::RightThumbX;
        value = ev.value;
        break;
    case SDL_CONTROLLER_AXIS_RIGHTY:
        axis_type = ControllerAxisEvent::AxisType::RightThumbY;
        value = ev.value;
        break;
    case SDL_CONTROLLER_AXIS_TRIGGERLEFT:
        axis_type = ControllerAxisEvent::AxisType::LeftTrigger;
        value = ev.value;
        break;
    case SDL_CONTROLLER_AXIS_TRIGGERRIGHT:
        axis_type = ControllerAxisEvent::AxisType::RightTrigger;
        value = ev.value;
        break;
    default:
        return;
    }
    onInputEvent(ControllerAxisEvent{index, axis_type, value});
}

void SdlInput::handleControllerButton(const SDL_ControllerButtonEvent& ev) {
    for (uint8_t index = 0; index < kMaxControllers; index++) {
        if (controller_states_[index].has_value() &&
            controller_states_[index]->joystick_id == ev.which) {
            onInputEvent(
                ControllerButtonEvent{index, static_cast<ControllerButtonEvent::Button>(ev.button),
                                      ev.state == SDL_PRESSED});
            return;
        }
    }
}

void SdlInput::handleControllerAdded(const SDL_ControllerDeviceEvent& ev) {
    SDL_GameController* controller = SDL_GameControllerOpen(ev.which);
    if (controller == NULL) {
        LOG(WARNING) << "Open controller failed: " << SDL_GetError();
        return;
    }
    uint8_t index;
    for (index = 0; index < kMaxControllers; index++) {
        if (!controller_states_[index].has_value()) {
            break;
        }
    }
    if (index >= kMaxControllers) {
        LOG(WARNING) << "Only support 4 controllers!";
        SDL_GameControllerClose(controller);
        return;
    }
    controller_states_[index] = ControllerState{};
    controller_states_[index]->index = index;
    controller_states_[index]->controller = controller;
    controller_states_[index]->joystick_id =
        SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(controller));
    SDL_GameControllerSetPlayerIndex(controller, index);

    char guid[kGUIDLength] = {0};
    SDL_JoystickGetGUIDString(SDL_JoystickGetGUID(SDL_GameControllerGetJoystick(controller)), guid,
                              kGUIDLength);
    guid[kGUIDLength - 1] = 0;
    const char* mapping = SDL_GameControllerMapping(controller);
    const char* name = SDL_GameControllerName(controller);
    LOGF(INFO, "Open controller:{name:%s, mapping:%s, guid:%s}", name, mapping, guid);
    if (mapping != nullptr) {
        SDL_free((void*)mapping);
    }

    onInputEvent(ControllerAddedRemovedEvent{index, true});
}

void SdlInput::handleControllerRemoved(const SDL_ControllerDeviceEvent& ev) {
    for (uint32_t index = 0; index < kMaxControllers; index++) {
        if (controller_states_[index].has_value() &&
            controller_states_[index]->joystick_id == ev.which) {
            SDL_GameControllerClose(controller_states_[index]->controller);
            controller_states_[index] = std::nullopt;
            onInputEvent(ControllerAddedRemovedEvent{index, /*is_added=*/false});
            return;
        }
    }
}

void SdlInput::handleJoystickAdded(const SDL_JoyDeviceEvent& ev) {
    if (SDL_IsGameController(ev.which)) {
        return;
    }
    char guid[kGUIDLength];
    SDL_JoystickGetGUIDString(SDL_JoystickGetDeviceGUID(ev.which), guid, kGUIDLength);
    const char* name = SDL_JoystickNameForIndex(ev.which);
    LOG(WARNING) << "Unknown controller: " << name;
}

void SdlInput::onInputEvent(const InputEvent& ev) {
    OnInputEvent handle_input;
    {
        std::lock_guard lock{mutex_};
        handle_input = on_input_event_;
    }
    if (handle_input) {
        handle_input(ev);
    }
}

} // namespace lt
