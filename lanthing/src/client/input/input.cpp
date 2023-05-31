#include "client/input/input.h"
#include "client/platforms/pc_sdl.h"
#include <array>
#include <g3log/g3log.hpp>
#include <ltproto/ltproto.h>
#include <ltproto/peer2peer/controller_added_removed.pb.h>
#include <ltproto/peer2peer/controller_status.pb.h>
#include <ltproto/peer2peer/keyboard_event.pb.h>
#include <ltproto/peer2peer/mouse_event.pb.h>

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
    int dst_height = dst.width * src.height / src.width;
    int dst_width = dst.height * src.width / src.height;
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

namespace cli {

class InputImpl {
public:
    InputImpl(const Input::Params& params);
    void init();

private:
    void send_message_to_host(uint32_t type,
                              const std::shared_ptr<google::protobuf::MessageLite>& msg,
                              bool reliable);
    void on_platform_input_event(const InputEvent& ev);
    bool try_process_key_combos();
    void handle_keyboard_up_down(const KeyboardEvent& ev);
    void handle_mouse_button(const MouseButtonEvent& ev);
    void handle_mouse_wheel(const MouseWheelEvent& ev);
    void handle_mouse_move(const MouseMoveEvent& ev);
    void handle_controller_added_removed(const ControllerAddedRemovedEvent& ev);
    void handle_controller_button(const ControllerButtonEvent& ev);
    void handle_controller_axis(const ControllerAxisEvent& ev);
    void send_controller_state(uint32_t index);

private:
    PcSdl* sdl_;
    std::function<void(uint32_t, const std::shared_ptr<google::protobuf::MessageLite>&, bool)>
        send_message_to_host_;
    // 0表示松开，非0表示按下。不用bool而用uint8_t是担心menset()之类函数不好处理bool数组
    std::array<uint8_t, 512> key_states_ = {0};
    uint32_t host_width_;
    uint32_t host_height_;
    std::array<std::optional<ControllerState>, 4> cstates_;
};

std::unique_ptr<Input> Input::create(const Params& params) {
    std::unique_ptr<Input> input{new Input};
    auto impl = std::make_unique<InputImpl>(params);
    impl->init();
    input->impl_ = std::move(impl);
    return input;
}

InputImpl::InputImpl(const Input::Params& params)
    : sdl_{params.sdl}
    , host_width_{params.host_width}
    , host_height_{params.host_height}
    , send_message_to_host_{params.send_message} {}

void InputImpl::init() {
    sdl_->set_input_handler(
        std::bind(&InputImpl::on_platform_input_event, this, std::placeholders::_1));
}

void InputImpl::send_message_to_host(uint32_t type,
                                     const std::shared_ptr<google::protobuf::MessageLite>& msg,
                                     bool reliable) {
    send_message_to_host_(type, msg, reliable);
}

void InputImpl::on_platform_input_event(const InputEvent& e) {
    switch (e.type) {
    case InputEventType::Keyboard:
        handle_keyboard_up_down(std::get<KeyboardEvent>(e.ev));
        break;
    case InputEventType::MouseButton:
        handle_mouse_button(std::get<MouseButtonEvent>(e.ev));
        break;
    case InputEventType::MouseMove:
        handle_mouse_move(std::get<MouseMoveEvent>(e.ev));
        break;
    case InputEventType::MouseWheel:
        handle_mouse_wheel(std::get<MouseWheelEvent>(e.ev));
        break;
    case InputEventType::ControllerAddedRemoved:
        handle_controller_added_removed(std::get<ControllerAddedRemovedEvent>(e.ev));
        break;
    case InputEventType::ControllerAxis:
        handle_controller_axis(std::get<ControllerAxisEvent>(e.ev));
        break;
    case InputEventType::ControllerButton:
        handle_controller_button(std::get<ControllerButtonEvent>(e.ev));
        break;
    default:
        LOG(FATAL) << "Unknown InputEventType:" << static_cast<int32_t>(e.type);
        break;
    }
}

void InputImpl::handle_keyboard_up_down(const KeyboardEvent& ev) {
    // TODO: 增加一个reset状态的逻辑，入口在sdl还是input另说。
    key_states_[ev.scan_code] = ev.is_pressed ? 1 : 0;
    if (try_process_key_combos()) {
        // 处理完预设的组合键后，直接返回掉不发给被控端
        return;
    }
    auto msg = std::make_shared<ltproto::peer2peer::KeyboardEvent>();
    msg->set_key(ev.scan_code);
    msg->set_down(ev.is_pressed);
    send_message_to_host(ltproto::id(msg), msg, true);
    LOG(DEBUG) << "Key:" << ev.scan_code << ", down:" << ev.is_pressed;
}

void InputImpl::handle_mouse_button(const MouseButtonEvent& ev) {
    Rect host_surface{host_width_, host_height_};
    Rect client_surface{ev.window_width, ev.window_height};
    Rect target_rect = ::scale_src_to_dst_surface(host_surface, client_surface);
    uint32_t padding_height = (client_surface.height - target_rect.height) / 2;
    uint32_t padding_width = (client_surface.width - target_rect.width) / 2;
    float x = (ev.x - padding_width) * 1.0f / target_rect.width;
    float y = (ev.y - padding_height) * 1.0f / target_rect.height;
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
    msg->set_x(x);
    msg->set_y(y);
    send_message_to_host(ltproto::id(msg), msg, true);
}

void InputImpl::handle_mouse_wheel(const MouseWheelEvent& ev) {
    auto msg = std::make_shared<ltproto::peer2peer::MouseEvent>();
    msg->set_delta_z(ev.amount);
    send_message_to_host(ltproto::id(msg), msg, true);
}

void InputImpl::handle_mouse_move(const MouseMoveEvent& ev) {
    Rect host_surface{host_width_, host_height_};
    Rect client_surface{ev.window_width, ev.window_height};
    Rect target_rect = ::scale_src_to_dst_surface(host_surface, client_surface);
    uint32_t padding_height = (client_surface.height - target_rect.height) / 2;
    uint32_t padding_width = (client_surface.width - target_rect.width) / 2;
    float x = (ev.x - padding_width) * 1.0f / target_rect.width;
    float y = (ev.y - padding_height) * 1.0f / target_rect.height;
    // TODO: 相对模式可能要累积一段再发出去
    auto msg = std::make_shared<ltproto::peer2peer::MouseEvent>();
    msg->set_x(x);
    msg->set_y(y);
    msg->set_delta_x(ev.delta_x);
    msg->set_delta_y(ev.delta_y);
    send_message_to_host(ltproto::id(msg), msg, true);
}

void InputImpl::handle_controller_added_removed(const ControllerAddedRemovedEvent& ev) {
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
    send_message_to_host(ltproto::id(msg), msg, true);
}

void InputImpl::handle_controller_button(const ControllerButtonEvent& ev) {
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
    send_controller_state(ev.index);
}

void InputImpl::handle_controller_axis(const ControllerAxisEvent& ev) {
    if (ev.index >= cstates_.size()) {
        return;
    }
    auto& state = cstates_[ev.index];
    if (!state.has_value()) {
        return;
    }
    switch (ev.axis_type) {
    case ControllerAxisEvent::AxisType::LeftTrigger:
        state->left_trigger = ev.value * 255UL / 32767;
        break;
    case ControllerAxisEvent::AxisType::RightTrigger:
        state->right_trigger = ev.value * 255UL / 32767;
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
    send_controller_state(ev.index);
}

void InputImpl::send_controller_state(uint32_t index) {
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
    send_message_to_host(ltproto::id(msg), msg, true);
}

bool InputImpl::try_process_key_combos() {
    return false;
}

} // namespace cli
} // namespace lt