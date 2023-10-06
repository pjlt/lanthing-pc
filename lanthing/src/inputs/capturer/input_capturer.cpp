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

#include <inputs/capturer/input_capturer.h>
#include <platforms/pc_sdl.h>

#include <array>

#include <ltlib/logging.h>
#include <ltproto/ltproto.h>
#include <ltproto/peer2peer/controller_added_removed.pb.h>
#include <ltproto/peer2peer/controller_status.pb.h>
#include <ltproto/peer2peer/keyboard_event.pb.h>
#include <ltproto/peer2peer/mouse_event.pb.h>

#include <inputs/executor/scancode.h>

namespace {

struct ControllerState {
    uint32_t buttons = 0;
    uint8_t left_trigger = 0;
    uint8_t right_trigger = 0;
    int16_t left_thumb_x = 0;
    int16_t left_thumb_y = 0;
    int16_t right_thumb_x = 0;
    int16_t right_thumb_y = 0;
};

constexpr uint32_t kControllerA = 0x1000;
constexpr uint32_t kControllerB = 0x2000;
constexpr uint32_t kControllerX = 0x4000;
constexpr uint32_t kControllerY = 0x8000;
constexpr uint32_t kControllerUp = 0x0001;
constexpr uint32_t kControllerDown = 0x0002;
constexpr uint32_t kControllerLeft = 0x0004;
constexpr uint32_t kControllerRight = 0x0008;
constexpr uint32_t kControllerStart = 0x0010;
constexpr uint32_t kControllerBack = 0x0020;
constexpr uint32_t kControllerLeftThumb = 0x0040;
constexpr uint32_t kControllerRightThumb = 0x0080;
constexpr uint32_t kControllerLeftShoulder = 0x0100;
constexpr uint32_t kControllerRightShoulder = 0x0200;

struct Rect {
    Rect(uint32_t w, uint32_t h)
        : width{w}
        , height{h} {}
    uint32_t width;
    uint32_t height;
};

Rect scale_src_to_dst_surface(Rect src, Rect dst) {
    uint32_t dst_height = dst.width * src.height / src.width;
    uint32_t dst_width = dst.height * src.width / src.height;
    if (dst_height < dst.height) {
        dst.height = dst_height;
    }
    else {
        dst.width = dst_width;
    }
    return dst;
}

} // namespace

namespace lt {

class InputCapturerImpl {
public:
    InputCapturerImpl(const InputCapturer::Params& params);
    void init();

private:
    void sendMessageToHost(uint32_t type, const std::shared_ptr<google::protobuf::MessageLite>& msg,
                           bool reliable);
    void onPlatformInputEvent(const InputEvent& ev);
    void handleKeyboardUpDown(const KeyboardEvent& ev);
    void handleMouseButton(const MouseButtonEvent& ev);
    void handleMouseWheel(const MouseWheelEvent& ev);
    void handleMouseMove(const MouseMoveEvent& ev);
    void handleControllerAddedRemoved(const ControllerAddedRemovedEvent& ev);
    void handleControllerButton(const ControllerButtonEvent& ev);
    void handleControllerAxis(const ControllerAxisEvent& ev);
    void sendControllerState(uint32_t index);
    void processHotKeys();

private:
    PcSdl* sdl_;
    std::function<void(uint32_t, const std::shared_ptr<google::protobuf::MessageLite>&, bool)>
        send_message_to_host_;
    std::function<void()> toggle_fullscreen_;
    // 0表示松开，非0表示按下。不用bool而用uint8_t是担心menset()之类函数不好处理bool数组
    std::array<uint8_t, 512> key_states_ = {0};
    uint32_t host_width_;
    uint32_t host_height_;
    std::array<std::optional<ControllerState>, 4> cstates_;
};

std::unique_ptr<InputCapturer> InputCapturer::create(const Params& params) {
    std::unique_ptr<InputCapturer> input{new InputCapturer};
    auto impl = std::make_unique<InputCapturerImpl>(params);
    impl->init();
    input->impl_ = std::move(impl);
    return input;
}

InputCapturerImpl::InputCapturerImpl(const InputCapturer::Params& params)
    : sdl_{params.sdl}
    , host_width_{params.host_width}
    , host_height_{params.host_height}
    , send_message_to_host_{params.send_message}
    , toggle_fullscreen_{params.toggle_fullscreen} {}

void InputCapturerImpl::init() {
    sdl_->setInputHandler(
        std::bind(&InputCapturerImpl::onPlatformInputEvent, this, std::placeholders::_1));
}

void InputCapturerImpl::sendMessageToHost(uint32_t type,
                                          const std::shared_ptr<google::protobuf::MessageLite>& msg,
                                          bool reliable) {
    send_message_to_host_(type, msg, reliable);
}

void InputCapturerImpl::onPlatformInputEvent(const InputEvent& e) {
    switch (e.type) {
    case InputEventType::Keyboard:
        handleKeyboardUpDown(std::get<KeyboardEvent>(e.ev));
        break;
    case InputEventType::MouseButton:
        handleMouseButton(std::get<MouseButtonEvent>(e.ev));
        break;
    case InputEventType::MouseMove:
        handleMouseMove(std::get<MouseMoveEvent>(e.ev));
        break;
    case InputEventType::MouseWheel:
        handleMouseWheel(std::get<MouseWheelEvent>(e.ev));
        break;
    case InputEventType::ControllerAddedRemoved:
        handleControllerAddedRemoved(std::get<ControllerAddedRemovedEvent>(e.ev));
        break;
    case InputEventType::ControllerAxis:
        handleControllerAxis(std::get<ControllerAxisEvent>(e.ev));
        break;
    case InputEventType::ControllerButton:
        handleControllerButton(std::get<ControllerButtonEvent>(e.ev));
        break;
    default:
        LOG(FATAL) << "Unknown InputEventType:" << static_cast<int32_t>(e.type);
        break;
    }
}

void InputCapturerImpl::handleKeyboardUpDown(const KeyboardEvent& ev) {
    // TODO: 增加一个reset状态的逻辑，入口在sdl还是input另说。
    key_states_[ev.scan_code] = ev.is_pressed ? 1 : 0;
    processHotKeys();
    auto msg = std::make_shared<ltproto::peer2peer::KeyboardEvent>();
    msg->set_key(ev.scan_code);
    msg->set_down(ev.is_pressed);
    sendMessageToHost(ltproto::id(msg), msg, true);
    LOG(DEBUG) << "Key:" << ev.scan_code << ", down:" << ev.is_pressed;
}

void InputCapturerImpl::handleMouseButton(const MouseButtonEvent& ev) {
    ltproto::peer2peer::MouseEvent::KeyFlag key_flag;
    switch (ev.button) {
    case MouseButtonEvent::Button::Left:
        key_flag = ev.is_pressed ? ltproto::peer2peer::MouseEvent_KeyFlag_LeftDown
                                 : ltproto::peer2peer::MouseEvent_KeyFlag_LeftUp;
        break;
    case MouseButtonEvent::Button::Mid:
        key_flag = ev.is_pressed ? ltproto::peer2peer::MouseEvent_KeyFlag_MidDown
                                 : ltproto::peer2peer::MouseEvent_KeyFlag_MidUp;
        break;
    case MouseButtonEvent::Button::Right:
        key_flag = ev.is_pressed ? ltproto::peer2peer::MouseEvent_KeyFlag_RightDown
                                 : ltproto::peer2peer::MouseEvent_KeyFlag_RightUp;
        break;
    case MouseButtonEvent::Button::X1:
        key_flag = ev.is_pressed ? ltproto::peer2peer::MouseEvent_KeyFlag_X1Down
                                 : ltproto::peer2peer::MouseEvent_KeyFlag_X1Up;
        break;
    case MouseButtonEvent::Button::X2:
        key_flag = ev.is_pressed ? ltproto::peer2peer::MouseEvent_KeyFlag_X2Down
                                 : ltproto::peer2peer::MouseEvent_KeyFlag_X2Up;
        break;
    default:
        LOG(FATAL) << "Unknown Mouse Button: " << static_cast<int32_t>(ev.button);
        return;
    }
    auto msg = std::make_shared<ltproto::peer2peer::MouseEvent>();
    msg->set_key_falg(key_flag);
    msg->set_x(ev.x * 1.0f / ev.window_width);
    msg->set_y(ev.y * 1.0f / ev.window_height);
    sendMessageToHost(ltproto::id(msg), msg, true);
}

void InputCapturerImpl::handleMouseWheel(const MouseWheelEvent& ev) {
    auto msg = std::make_shared<ltproto::peer2peer::MouseEvent>();
    msg->set_delta_z(ev.amount);
    sendMessageToHost(ltproto::id(msg), msg, true);
}

void InputCapturerImpl::handleMouseMove(const MouseMoveEvent& ev) {

    // TODO: 相对模式可能要累积一段再发出去
    auto msg = std::make_shared<ltproto::peer2peer::MouseEvent>();
    msg->set_x(ev.x * 1.0f / ev.window_width);
    msg->set_y(ev.y * 1.0f / ev.window_height);
    msg->set_delta_x(ev.delta_x);
    msg->set_delta_y(ev.delta_y);
    sendMessageToHost(ltproto::id(msg), msg, true);
}

void InputCapturerImpl::handleControllerAddedRemoved(const ControllerAddedRemovedEvent& ev) {
    if (ev.index >= cstates_.size()) {
        return;
    }
    auto msg = std::make_shared<ltproto::peer2peer::ControllerAddedRemoved>();
    msg->set_index(ev.index);
    msg->set_is_added(ev.is_added);
    if (ev.is_added) {
        cstates_[ev.index] = ControllerState{};
    }
    else {
        cstates_[ev.index] = std::nullopt;
    }
    sendMessageToHost(ltproto::id(msg), msg, true);
}

void InputCapturerImpl::handleControllerButton(const ControllerButtonEvent& ev) {
    if (ev.index >= cstates_.size()) {
        return;
    }
    auto& state = cstates_[ev.index];
    if (!state.has_value()) {
        return;
    }
    if (ev.is_pressed) {
        switch (ev.button) {
        case ControllerButtonEvent::Button::A:
            state->buttons |= kControllerA;
            break;
        case ControllerButtonEvent::Button::B:
            state->buttons |= kControllerB;
            break;
        case ControllerButtonEvent::Button::X:
            state->buttons |= kControllerX;
            break;
        case ControllerButtonEvent::Button::Y:
            state->buttons |= kControllerY;
            break;
        case ControllerButtonEvent::Button::BACK:
            state->buttons |= kControllerBack;
            break;
        case ControllerButtonEvent::Button::GUIDE: // 不确定
            break;
        case ControllerButtonEvent::Button::START:
            state->buttons |= kControllerStart;
            break;
        case ControllerButtonEvent::Button::LEFTSTICK:
            state->buttons |= kControllerLeftThumb;
            break;
        case ControllerButtonEvent::Button::RIGHTSTICK:
            state->buttons |= kControllerRightThumb;
            break;
        case ControllerButtonEvent::Button::LEFTSHOULDER:
            state->buttons |= kControllerLeftShoulder;
            break;
        case ControllerButtonEvent::Button::RIGHTSHOULDER:
            state->buttons |= kControllerRightShoulder;
            break;
        case ControllerButtonEvent::Button::DPAD_UP:
            state->buttons |= kControllerUp;
            break;
        case ControllerButtonEvent::Button::DPAD_DOWN:
            state->buttons |= kControllerDown;
            break;
        case ControllerButtonEvent::Button::DPAD_LEFT:
            state->buttons |= kControllerLeft;
            break;
        case ControllerButtonEvent::Button::DPAD_RIGHT:
            state->buttons |= kControllerRight;
            break;
        default:
            return;
        }
    }
    else {
        switch (ev.button) {
        case ControllerButtonEvent::Button::A:
            state->buttons &= ~kControllerA;
            break;
        case ControllerButtonEvent::Button::B:
            state->buttons &= ~kControllerB;
            break;
        case ControllerButtonEvent::Button::X:
            state->buttons &= ~kControllerX;
            break;
        case ControllerButtonEvent::Button::Y:
            state->buttons &= ~kControllerY;
            break;
        case ControllerButtonEvent::Button::BACK:
            state->buttons &= ~kControllerBack;
            break;
        case ControllerButtonEvent::Button::GUIDE: // 不确定
            break;
        case ControllerButtonEvent::Button::START:
            state->buttons &= ~kControllerStart;
            break;
        case ControllerButtonEvent::Button::LEFTSTICK:
            state->buttons &= ~kControllerLeftThumb;
            break;
        case ControllerButtonEvent::Button::RIGHTSTICK:
            state->buttons &= ~kControllerRightThumb;
            break;
        case ControllerButtonEvent::Button::LEFTSHOULDER:
            state->buttons &= ~kControllerLeftShoulder;
            break;
        case ControllerButtonEvent::Button::RIGHTSHOULDER:
            state->buttons &= ~kControllerRightShoulder;
            break;
        case ControllerButtonEvent::Button::DPAD_UP:
            state->buttons &= ~kControllerUp;
            break;
        case ControllerButtonEvent::Button::DPAD_DOWN:
            state->buttons &= ~kControllerDown;
            break;
        case ControllerButtonEvent::Button::DPAD_LEFT:
            state->buttons &= ~kControllerLeft;
            break;
        case ControllerButtonEvent::Button::DPAD_RIGHT:
            state->buttons &= ~kControllerRight;
            break;
        default:
            return;
        }
    }
    sendControllerState(ev.index);
}

void InputCapturerImpl::handleControllerAxis(const ControllerAxisEvent& ev) {
    if (ev.index >= cstates_.size()) {
        return;
    }
    auto& state = cstates_[ev.index];
    if (!state.has_value()) {
        return;
    }
    switch (ev.axis_type) {
    case ControllerAxisEvent::AxisType::LeftTrigger:
        state->left_trigger = static_cast<uint8_t>(ev.value * 255UL / 32767);
        break;
    case ControllerAxisEvent::AxisType::RightTrigger:
        state->right_trigger = static_cast<uint8_t>(ev.value * 255UL / 32767);
        break;
    case ControllerAxisEvent::AxisType::LeftThumbX:
        state->left_thumb_x = ev.value;
        break;
    case ControllerAxisEvent::AxisType::LeftThumbY:
        state->left_thumb_y = -std::max(ev.value, (int16_t)-32767);
        break;
    case ControllerAxisEvent::AxisType::RightThumbX:
        state->right_thumb_x = ev.value;
        break;
    case ControllerAxisEvent::AxisType::RightThumbY:
        state->right_thumb_y = -std::max(ev.value, (int16_t)-32767);
        break;
    default:
        return;
    }
    sendControllerState(ev.index);
}

void InputCapturerImpl::sendControllerState(uint32_t index) {
    if (index >= cstates_.size()) {
        return;
    }
    auto& state = cstates_[index];
    if (!state.has_value()) {
        return;
    }
    auto msg = std::make_shared<ltproto::peer2peer::ControllerStatus>();
    msg->set_button_flags(state->buttons);
    msg->set_gamepad_index(index);
    msg->set_left_stick_x(state->left_thumb_x);
    msg->set_left_stick_y(state->left_thumb_y);
    msg->set_right_stick_x(state->right_thumb_x);
    msg->set_right_stick_y(state->right_thumb_y);
    msg->set_left_trigger(state->left_trigger);
    msg->set_right_trigger(state->right_trigger);
    sendMessageToHost(ltproto::id(msg), msg, true);
}

void InputCapturerImpl::processHotKeys() {
    // TODO: 按键释放问题
    if (key_states_[Scancode::SCANCODE_LGUI] && key_states_[Scancode::SCANCODE_LSHIFT] &&
        key_states_[Scancode::SCANCODE_Z]) {
        toggle_fullscreen_();
    }
}

} // namespace lt