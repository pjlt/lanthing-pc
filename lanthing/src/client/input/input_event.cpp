#include "client/input/input_event.h"

namespace lt {

namespace cli {

InputEvent::InputEvent(const KeyboardEvent& kb)
    : type{InputEventType::Keyboard}
    , ev{kb} {}

InputEvent::InputEvent(const MouseButtonEvent& mouse)
    : type{InputEventType::MouseButton}
    , ev{mouse} {}

InputEvent::InputEvent(const MouseMoveEvent& mouse)
    : type{InputEventType::MouseMove}
    , ev{mouse} {}

InputEvent::InputEvent(const MouseWheelEvent& mouse)
    : type{InputEventType::MouseWheel}
    , ev{mouse} {}

InputEvent::InputEvent(const ControllerAddedRemovedEvent& controller)
    : type{InputEventType::ControllerAddedRemoved}
    , ev{controller} {}

InputEvent::InputEvent(const ControllerButtonEvent& controlelr)
    : type{InputEventType::ControllerButton}
    , ev{controlelr} {}

InputEvent::InputEvent(const ControllerAxisEvent& controller)
    : type{InputEventType::ControllerAxis}
    , ev{controller} {}

} // namespace cli

} // namespace lt