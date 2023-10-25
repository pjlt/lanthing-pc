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

#pragma once

#include <cstdint>

#include <functional>
#include <memory>
#include <mutex>

#include <google/protobuf/message_lite.h>

#include <message_handler.h>

namespace lt {

class Gamepad;
class InputExecutor {
public:
    enum class Type : uint8_t {
        WIN32_MESSAGE = 1,
        WIN32_DRIVER = 2,
    };
    struct Params {
        uint8_t types = 0;
        uint32_t screen_width = 0;
        uint32_t screen_height = 0;
        std::function<bool(uint32_t, const MessageHandler&)> register_message_handler;
        std::function<bool(uint32_t, const std::shared_ptr<google::protobuf::MessageLite>&)>
            send_message;
    };

public:
    static std::unique_ptr<InputExecutor> create(const Params& params);
    virtual ~InputExecutor() = default;

    void switchMouseMode(bool absolute);

protected:
    virtual bool initKeyMouse() = 0;
    virtual void onMouseEvent(const std::shared_ptr<google::protobuf::MessageLite>&) = 0;
    virtual void onKeyboardEvent(const std::shared_ptr<google::protobuf::MessageLite>&) = 0;

    void sendMessage(uint32_t, const std::shared_ptr<google::protobuf::MessageLite>& msg);
    bool isAbsoluteMouse() const;

private:
    bool init();
    bool registerHandlers();
    void onControllerAddedRemoved(const std::shared_ptr<google::protobuf::MessageLite>& msg);
    void onControllerStatus(const std::shared_ptr<google::protobuf::MessageLite>& msg);
    // void onSwitchMouseMode(const std::shared_ptr<google::protobuf::MessageLite>& msg);
    void onGamepadResponse(uint32_t index, uint16_t large_motor, uint16_t small_motor);

private:
    std::function<bool(uint32_t, const MessageHandler&)> register_message_handler_;
    std::function<bool(uint32_t, const std::shared_ptr<google::protobuf::MessageLite>&)>
        send_message_;
    std::mutex mutex_;
    bool is_absolute_mouse_ = true;
    std::shared_ptr<Gamepad> gamepad_;
};

} // namespace lt
