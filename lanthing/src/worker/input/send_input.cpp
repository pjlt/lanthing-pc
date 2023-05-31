#include "send_input.h"

#include <Windows.h>
#include <winuser.h>

#include <optional>

#include <g3log/g3log.hpp>

#include <ltproto/peer2peer/keyboard_event.pb.h>
#include <ltproto/peer2peer/mouse_event.pb.h>

namespace lt {
namespace worker {

void Win32SendInput::onKeyboardEvent(const std::shared_ptr<google::protobuf::MessageLite>& msg) {
    auto keyboard = std::static_pointer_cast<ltproto::peer2peer::KeyboardEvent>(msg);
    INPUT inputs[1] = {};
    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wScan = static_cast<WORD>(keyboard->key());
    inputs[0].ki.wVk = 0;
    inputs[0].ki.dwFlags |= KEYEVENTF_SCANCODE;
    if (!keyboard->down()) {
        inputs[0].ki.dwFlags |= KEYEVENTF_KEYUP;
    }
    // FIXME: implement it;
    inputs[0].ki.time = 0;
    SendInput(ARRAYSIZE(inputs), inputs, sizeof(INPUT));
}

bool Win32SendInput::initKeyMouse() {
    return true;
}

void Win32SendInput::onMouseEvent(const std::shared_ptr<google::protobuf::MessageLite>& msg) {
    auto mouse = std::static_pointer_cast<ltproto::peer2peer::MouseEvent>(msg);
    INPUT inputs[1] = {};
    inputs[0].type = INPUT_MOUSE;
    if (mouse->has_key_falg()) {
        switch (mouse->key_falg()) {
        case ltproto::peer2peer::MouseEvent_KeyFlag_LeftDown:
            inputs[0].mi.dwFlags |= MOUSEEVENTF_LEFTDOWN;
            break;
        case ltproto::peer2peer::MouseEvent_KeyFlag_LeftUp:
            inputs[0].mi.dwFlags |= MOUSEEVENTF_LEFTUP;
            break;
        case ltproto::peer2peer::MouseEvent_KeyFlag_RightDown:
            inputs[0].mi.dwFlags |= MOUSEEVENTF_RIGHTDOWN;
            break;
        case ltproto::peer2peer::MouseEvent_KeyFlag_RightUp:
            inputs[0].mi.dwFlags |= MOUSEEVENTF_RIGHTUP;
            break;
        case ltproto::peer2peer::MouseEvent_KeyFlag_MidDown:
            inputs[0].mi.dwFlags |= MOUSEEVENTF_MIDDLEDOWN;
            break;
        case ltproto::peer2peer::MouseEvent_KeyFlag_MidUp:
            inputs[0].mi.dwFlags |= MOUSEEVENTF_MIDDLEUP;
            break;
        case ltproto::peer2peer::MouseEvent_KeyFlag_X1Down:
            inputs[0].mi.mouseData = XBUTTON1;
            inputs[0].mi.dwFlags |= MOUSEEVENTF_XDOWN;
            break;
        case ltproto::peer2peer::MouseEvent_KeyFlag_X1Up:
            inputs[0].mi.mouseData = XBUTTON1;
            inputs[0].mi.dwFlags |= MOUSEEVENTF_XUP;
            break;
        case ltproto::peer2peer::MouseEvent_KeyFlag_X2Down:
            inputs[0].mi.mouseData = XBUTTON2;
            inputs[0].mi.dwFlags |= MOUSEEVENTF_XDOWN;
            break;
        case ltproto::peer2peer::MouseEvent_KeyFlag_X2Up:
            inputs[0].mi.mouseData = XBUTTON2;
            inputs[0].mi.dwFlags |= MOUSEEVENTF_XUP;
            break;
        default:
            // LOG(WARNING) << "";
            break;
        }
    }

    if (isAbsoluteMouse()) {
        if (mouse->has_x() || mouse->has_y()) {
            inputs[0].mi.dx = static_cast<LONG>((65535.0f * mouse->x()));
            inputs[0].mi.dy = static_cast<LONG>((65535.0f * mouse->y()));
            inputs[0].mi.dwFlags |= MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_MOVE;
        }
    }
    else {
        if (mouse->has_delta_x() || mouse->has_delta_y()) {
            inputs[0].mi.dx = mouse->delta_x();
            inputs[0].mi.dy = mouse->delta_y();
            inputs[0].mi.dwFlags |= MOUSEEVENTF_MOVE;
        }
    }
    if (mouse->has_delta_z()) {
        inputs[0].mi.mouseData = mouse->delta_z();
        inputs[0].mi.dwFlags = MOUSEEVENTF_WHEEL;
    }

    // FIXME: implement it;
    inputs[0].mi.time = 0;
    SendInput(ARRAYSIZE(inputs), inputs, sizeof(INPUT));
}

} // namespace worker
} // namespace lt
