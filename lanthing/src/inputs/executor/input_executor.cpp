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

#include <Windows.h>

#include "input_executor.h"

#include <Xinput.h>

#include <ViGEm/Common.h>
#include <ltlib/logging.h>

#include <inputs/executor/gamepad.h>
#include <ltproto/ltproto.h>
#include <ltproto/peer2peer/controller_added_removed.pb.h>
#include <ltproto/peer2peer/controller_response.pb.h>
#include <ltproto/peer2peer/controller_status.pb.h>

#include "win_send_input.h"

namespace lt {

std::unique_ptr<InputExecutor> InputExecutor::create(const Params& params) {
    if (params.register_message_handler == nullptr || params.send_message == nullptr) {
        return nullptr;
    }
    std::unique_ptr<InputExecutor> input;
    if (params.types & static_cast<uint8_t>(Type::WIN32_MESSAGE)) {
        input = std::make_unique<Win32SendInput>(params.screen_width, params.screen_height);
    }
    else {
        return nullptr;
    }
    input->register_message_handler_ = params.register_message_handler;
    input->send_message_ = params.send_message;
    if (!input->init()) {
        return nullptr;
    }
    return input;
}

bool InputExecutor::init() {
    if (!registerHandlers()) {
        return false;
    }
    if (!initKeyMouse()) {
        return false;
    }
    gamepad_ =
        Gamepad::create(std::bind(&InputExecutor::onGamepadResponse, this, std::placeholders::_1,
                                  std::placeholders::_2, std::placeholders::_3));
    if (gamepad_ == nullptr) {
        return false;
    }
    return true;
}

bool InputExecutor::registerHandlers() {
    namespace ltype = ltproto::type;
    namespace ph = std::placeholders;
    const std::pair<uint32_t, MessageHandler> handlers[] = {
        {ltype::kMouseEvent, std::bind(&InputExecutor::onMouseEvent, this, ph::_1)},
        {ltype::kKeyboardEvent, std::bind(&InputExecutor::onKeyboardEvent, this, ph::_1)},
        {ltype::kControllerAddedRemoved,
         std::bind(&InputExecutor::onControllerAddedRemoved, this, ph::_1)},
        {ltype::kControllerStatus, std::bind(&InputExecutor::onControllerStatus, this, ph::_1)}};
    for (auto& handler : handlers) {
        if (!register_message_handler_(handler.first, handler.second)) {
            return false;
        }
    }
    return true;
};

void InputExecutor::sendMessagte(uint32_t type,
                                 const std::shared_ptr<google::protobuf::MessageLite>& msg) {
    send_message_(type, msg);
}

bool InputExecutor::isAbsoluteMouse() const {
    return is_absolute_mouse_;
}

void InputExecutor::onControllerAddedRemoved(
    const std::shared_ptr<google::protobuf::MessageLite>& msg) {
    auto controller = std::static_pointer_cast<ltproto::peer2peer::ControllerAddedRemoved>(msg);
    if (controller->is_added()) {
        gamepad_->plugin(controller->index());
    }
    else {
        gamepad_->plugout(controller->index());
    }
}

void InputExecutor::onControllerStatus(const std::shared_ptr<google::protobuf::MessageLite>& msg) {
    auto controller = std::static_pointer_cast<ltproto::peer2peer::ControllerStatus>(msg);
    if (controller->gamepad_index() >= XUSER_MAX_COUNT) {
        LOG(ERR) << "Gamepad index exceed limit: " << controller->gamepad_index();
        return;
    }
    XUSB_REPORT gamepad_report{};
    gamepad_report.wButtons = static_cast<USHORT>(controller->button_flags());
    gamepad_report.bLeftTrigger = static_cast<BYTE>(controller->left_trigger()); // 0-255
    gamepad_report.bRightTrigger = static_cast<BYTE>(controller->right_trigger());
    gamepad_report.sThumbLX = static_cast<USHORT>(controller->left_stick_x()); //-32768~ 32767
    gamepad_report.sThumbLY = static_cast<USHORT>(controller->left_stick_y());
    gamepad_report.sThumbRX = static_cast<USHORT>(controller->right_stick_x());
    gamepad_report.sThumbRY = static_cast<USHORT>(controller->right_stick_y());
    gamepad_->submit(controller->gamepad_index(), gamepad_report);
}

void InputExecutor::onGamepadResponse(uint32_t index, uint16_t large_motor, uint16_t small_motor) {
    auto controller = std::make_shared<ltproto::peer2peer::ControllerResponse>();
    controller->set_gamepad_index(index);
    controller->set_large_motor(large_motor);
    controller->set_small_moror(small_motor);
    sendMessagte(ltproto::id(controller), controller);
}

} // namespace lt
