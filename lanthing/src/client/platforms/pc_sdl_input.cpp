#include <g3log/g3log.hpp>
#include "pc_sdl_input.h"

namespace
{

constexpr size_t kGUIDLength = 64;

} // namespace

namespace lt
{

namespace cli
{
std::unique_ptr<SdlInput> SdlInput::create(const Params& params)
{
    std::unique_ptr<SdlInput> input { new SdlInput { params } };
    input->init();
    return input;
}

SdlInput::SdlInput(const Params& params)
    : window_ { params.window }
{
    //TODO: 初始化
}

void SdlInput::init()
{
    //TODO: 加载通过某种方式嵌入二进制文件的gamemapping文件
    //NOTE: 这里只有mapping_cout > 0这个分支条件是正常，其他都是为错误！
    std::string mapping_contents;
    if (!mapping_contents.empty()) {
        int mapping_count = SDL_GameControllerAddMappingsFromRW(SDL_RWFromConstMem(mapping_contents.c_str(), mapping_contents.size()), true);
        if (mapping_count > 0) {
            LOGF(INFO, "Successfully load %d controller mappings", mapping_count);
        } else if (mapping_count == 0) {
            LOG(WARNING) << "No controller mappings loaded";
        } else {
            LOG(WARNING) << "Map controller data failed";
        }
    } else {
        LOG(WARNING) << "No controller mappings data found";
    }
}

void SdlInput::set_input_handler(const OnInputEvent& on_input_event)
{
    std::lock_guard lock { mutex_ };
    on_input_event_ = on_input_event;
}


void SdlInput::handle_key_up_down(const SDL_KeyboardEvent& ev)
{
    if (ev.repeat) {
        return;
    }
    //这个范围里并不是每一个数值都有对应的SDL Scancode，进一步过滤的逻辑交由业务代码去做！
    if (ev.keysym.scancode <= SDL_SCANCODE_UNKNOWN || ev.keysym.scancode >= SDL_NUM_SCANCODES) {
        return;
    }
    on_input_event(KeyboardEvent { static_cast<uint16_t>(ev.keysym.scancode), ev.type == SDL_KEYDOWN });
    //auto old_state_is_pressed = keyboard_state_[ev.keysym.scancode] == 0 ? false : true;
    //auto new_state_is_pressed = ev.type == SDL_KEYUP ? false : true;
    //if (old_state_is_pressed != new_state_is_pressed) {
    //    keyboard_state_[ev.keysym.scancode] = new_state_is_pressed;
    //    keys_to_send = ({ev.keysym.scancode, new_state_is_pressed});
    //}
}

void SdlInput::handle_mouse_button(const SDL_MouseButtonEvent& ev)
{
    //SdlInput属于platform层，只负责把窗口内所有鼠标button事件回调到业务层的Input
    //渲染的视频可能只是这个窗口的一部分，判断点击是否在窗口内的逻辑，由业务层去做
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
        //SDL会不会出bug？
        return;
    }
    on_input_event(MouseButtonEvent { btn, ev.state == SDL_PRESSED, ev.x, ev.y });
}

void SdlInput::handle_mouse_move(const SDL_MouseMotionEvent& ev)
{
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
    on_input_event(MouseMoveEvent { ev.x, ev.y, ev.xrel, ev.yrel, static_cast<uint32_t>(width), static_cast<uint32_t>(height) });
}

void SdlInput::handle_mouse_wheel(const SDL_MouseWheelEvent& ev)
{
    if (ev.which == SDL_TOUCH_MOUSEID) {
        return;
    }
    on_input_event(MouseWheelEvent { ev.y * 120 });
}

void SdlInput::handle_controller_axis(const SDL_ControllerAxisEvent& ev)
{
    uint8_t index;
    for (index = 0; index < kMaxControllers; index++) {
        if (controller_states_[index].has_value() && controller_states_[index]->joystick_id == ev.which) {
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
    on_input_event(ControllerAxisEvent { index, axis_type, value });
}

void SdlInput::handle_controller_button(const SDL_ControllerButtonEvent& ev)
{
    for (uint8_t index = 0; index < kMaxControllers; index++) {
        if (controller_states_[index].has_value() && controller_states_[index]->joystick_id == ev.which) {
            on_input_event(ControllerButtonEvent { index, static_cast<ControllerButtonEvent::Button>(ev.button), ev.state == SDL_PRESSED });
            return;
        }
    }
}

void SdlInput::handle_controller_added(const SDL_ControllerDeviceEvent& ev)
{
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
    controller_states_[index] = ControllerState {};
    controller_states_[index]->index = index;
    controller_states_[index]->controller = controller;
    controller_states_[index]->joystick_id = SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(controller));
    SDL_GameControllerSetPlayerIndex(controller, index);

    char guid[kGUIDLength] = {0};
    SDL_JoystickGetGUIDString(SDL_JoystickGetGUID(SDL_GameControllerGetJoystick(controller)), guid, kGUIDLength);
    guid[kGUIDLength - 1] = 0;
    const char* mapping = SDL_GameControllerMapping(controller);
    const char* name = SDL_GameControllerName(controller);
    LOGF(INFO, "Open controller:{name:%s, mapping:%s, guid:%s}", name, mapping, guid);
    if (mapping != nullptr) {
        SDL_free((void*)mapping);
    }

    on_input_event(ControllerAddedRemovedEvent { index, true });
}

void SdlInput::handle_controller_removed(const SDL_ControllerDeviceEvent& ev)
{
    for (uint32_t index = 0; index < kMaxControllers; index++) {
        if (controller_states_[index].has_value() && controller_states_[index]->joystick_id == ev.which) {
            SDL_GameControllerClose(controller_states_[index]->controller);
            controller_states_[index] = std::nullopt;
            on_input_event(ControllerAddedRemovedEvent { index, /*is_added=*/ false });
            return;
        }
    }
}

void SdlInput::handle_joystick_added(const SDL_JoyDeviceEvent& ev)
{
    if (SDL_IsGameController(ev.which)) {
        return;
    }
    char guid[kGUIDLength];
    SDL_JoystickGetGUIDString(SDL_JoystickGetDeviceGUID(ev.which), guid, kGUIDLength);
    const char* name = SDL_JoystickNameForIndex(ev.which);
    LOG(WARNING) << "Unknown controller: " << name;
}

void SdlInput::on_input_event(const InputEvent& ev)
{
    OnInputEvent handle_input;
    {
        std::lock_guard lock { mutex_ };
        handle_input = on_input_event_;
    }
    if (handle_input) {
        handle_input(ev);
    }
}

} // namespace cli

} // namespace lt
