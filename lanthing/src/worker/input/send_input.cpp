#include "send_input.h"

#include <Windows.h>
#include <winuser.h>

namespace lt {
namespace worker {

void Win32SendInput::onEvent(const std::shared_ptr<ltproto::peer2peer::KeyboardEvent>& keyboard) {
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

void Win32SendInput::onEvent(const std::shared_ptr<ltproto::peer2peer::MouseClick>& mouse) {
    INPUT inputs[1] = {};
    inputs[0].type = INPUT_MOUSE;
    inputs[0].mi.mouseData = 0;
    inputs[0].mi.dx = static_cast<LONG>(mouse->x() * screen_width_);
    inputs[0].mi.dy = static_cast<LONG>(mouse->y() * screen_height_);
    switch (mouse->key_falg()) {
    case ltproto::peer2peer::MouseClick_KeyFlag_LeftDown:
        inputs[0].mi.dwFlags |= MOUSEEVENTF_LEFTDOWN;
        break;
    case ltproto::peer2peer::MouseClick_KeyFlag_LeftUp:
        inputs[0].mi.dwFlags |= MOUSEEVENTF_LEFTUP;
        break;
    case ltproto::peer2peer::MouseClick_KeyFlag_RightDown:
        inputs[0].mi.dwFlags |= MOUSEEVENTF_RIGHTDOWN;
        break;
    case ltproto::peer2peer::MouseClick_KeyFlag_RightUp:
        inputs[0].mi.dwFlags |= MOUSEEVENTF_RIGHTUP;
        break;
    case ltproto::peer2peer::MouseClick_KeyFlag_MidDown:
        inputs[0].mi.dwFlags |= MOUSEEVENTF_MIDDLEDOWN;
        break;
    case ltproto::peer2peer::MouseClick_KeyFlag_MidUp:
        inputs[0].mi.dwFlags |= MOUSEEVENTF_MIDDLEUP;
        break;
    case ltproto::peer2peer::MouseClick_KeyFlag_X1Down:
        inputs[0].mi.mouseData = XBUTTON1;
        inputs[0].mi.dwFlags |= MOUSEEVENTF_XDOWN;
        break;
    case ltproto::peer2peer::MouseClick_KeyFlag_X1Up:
        inputs[0].mi.mouseData = XBUTTON1;
        inputs[0].mi.dwFlags |= MOUSEEVENTF_XUP;
        break;
    case ltproto::peer2peer::MouseClick_KeyFlag_X2Down:
        inputs[0].mi.mouseData = XBUTTON2;
        inputs[0].mi.dwFlags |= MOUSEEVENTF_XDOWN;
        break;
    case ltproto::peer2peer::MouseClick_KeyFlag_X2Up:
        inputs[0].mi.mouseData = XBUTTON2;
        inputs[0].mi.dwFlags |= MOUSEEVENTF_XUP;
        break;
    default:
        return;
    }
    // FIXME: implement it;
    inputs[0].mi.time = 0;
    SendInput(ARRAYSIZE(inputs), inputs, sizeof(INPUT));
}

void Win32SendInput::onEvent(const std::shared_ptr<ltproto::peer2peer::MouseMotion>& mouse) {
    INPUT inputs[1] = {};
    inputs[0].type = INPUT_MOUSE;
    inputs[0].mi.dwFlags |= MOUSEEVENTF_MOVE;
    inputs[0].mi.mouseData = 0;
    inputs[0].mi.dx = static_cast<LONG>(mouse->x() * screen_width_);
    inputs[0].mi.dy = static_cast<LONG>(mouse->y() * screen_height_);
    // FIXME: implement it;
    inputs[0].mi.time = 0;
    SendInput(ARRAYSIZE(inputs), inputs, sizeof(INPUT));
}

void Win32SendInput::onEvent(const std::shared_ptr<ltproto::peer2peer::MouseWheel>& mouse) {
    INPUT inputs[1] = {};
    inputs[0].type = INPUT_MOUSE;
    inputs[0].mi.dwFlags |= MOUSEEVENTF_WHEEL;
    inputs[0].mi.mouseData = static_cast<DWORD>(mouse->amount());
    // FIXME: implement it;
    inputs[0].mi.time = 0;
    SendInput(ARRAYSIZE(inputs), inputs, sizeof(INPUT));
}
} // namespace worker
} // namespace lt
