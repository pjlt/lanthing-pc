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

#include <inputs/capturer/input_event.h>

namespace lt {

namespace input {

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

} // namespace input

} // namespace lt