#pragma once
#include <functional>
#include <variant>

namespace lt
{

namespace cli
{

struct KeyboardEvent
{
    KeyboardEvent(uint16_t _code, bool pressed)
        : scan_code { _code }
        , is_pressed { pressed }
    {
    }
    uint16_t scan_code;
    bool is_pressed;
};

struct MouseButtonEvent
{
    enum class Button : uint8_t
    {
        Left,
        Mid,
        Right,
        X1,
        X2,
    };
    MouseButtonEvent(Button btn, bool pressed, int32_t _x, int32_t _y)
        : button {btn}
        , is_pressed { pressed }
        , x {_x}
        , y {_y}
    {
    }
    Button button;
    bool is_pressed;
    int32_t x; // relative to window
    int32_t y; // relative to window
};

struct MouseMoveEvent
{
    MouseMoveEvent(int32_t _x, int32_t _y, int32_t dx, int32_t dy, uint32_t width, uint32_t height)
        : x {_x}
        , y {_y}
        , delta_x { dx }
        , delta_y { dy }
        , window_width { width }
        , window_height { height }
    {
    }
    int32_t x;
    int32_t y;
    int32_t delta_x;
    int32_t delta_y;
    uint32_t window_width;
    uint32_t window_height;
};

struct MouseWheelEvent
{
    explicit MouseWheelEvent(int32_t _amount)
        : amount { _amount }
    {}
    int32_t amount;
};

struct ControllerAddedRemovedEvent
{
    ControllerAddedRemovedEvent(uint32_t idx, bool added)
        : index { idx }
        , is_added { added }
    {
    }
    uint32_t index;
    bool is_added;
};

struct ControllerButtonEvent
{
    enum class Button
    {
        Invalid = -1,
        A,
        B,
        X,
        Y,
        BACK,
        GUIDE,
        START,
        LEFTSTICK,
        RIGHTSTICK,
        LEFTSHOULDER,
        RIGHTSHOULDER,
        DPAD_UP,
        DPAD_DOWN,
        DPAD_LEFT,
        DPAD_RIGHT,
        MISC1, /* Xbox Series X share button, PS5 microphone button, Nintendo Switch Pro capture button, Amazon Luna microphone button */
        PADDLE1, /* Xbox Elite paddle P1 */
        PADDLE2, /* Xbox Elite paddle P3 */
        PADDLE3, /* Xbox Elite paddle P2 */
        PADDLE4, /* Xbox Elite paddle P4 */
        TOUCHPAD, /* PS4/PS5 touchpad button */
        MAX
    };
    ControllerButtonEvent(uint8_t idx, Button btn, bool pressed)
        : index { idx }
        , button { btn }
        , is_pressed { pressed }
    {
    }
    uint8_t index;
    Button button;
    bool is_pressed;
};

struct ControllerAxisEvent
{
    enum class AxisType
    {
        LeftTrigger,
        RightTrigger,
        LeftThumbX,
        LeftThumbY,
        RightThumbX,
        RightThumbY,
    };
    ControllerAxisEvent(uint8_t idx, AxisType type, int16_t val)
        : index { idx }
        , axis_type { type }
        , value { val }
    {
    }
    uint8_t index;
    AxisType axis_type;
    int16_t value;
};

enum class InputEventType
{
    Keyboard,
    MouseButton,
    MouseMove,
    MouseWheel,
    ControllerAddedRemoved,
    ControllerButton,
    ControllerAxis,
};

struct InputEvent
{
    using UnionEvent = std::variant<
        KeyboardEvent,
        MouseButtonEvent,
        MouseMoveEvent,
        MouseWheelEvent,
        ControllerAddedRemovedEvent,
        ControllerButtonEvent,
        ControllerAxisEvent
    >;
    InputEvent(const KeyboardEvent& kb);
    InputEvent(const MouseButtonEvent& mouse);
    InputEvent(const MouseMoveEvent& mouse);
    InputEvent(const MouseWheelEvent& mouse);
    InputEvent(const ControllerAddedRemovedEvent& controller);
    InputEvent(const ControllerButtonEvent& controller);
    InputEvent(const ControllerAxisEvent& controller);
    const InputEventType type;
    const UnionEvent ev;
};

using OnInputEvent = std::function<void(const InputEvent&)>;

} // namespace cli

} // namespace lt