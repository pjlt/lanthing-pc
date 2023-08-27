#pragma once

#include <cstdint>
#include <functional>
#include <memory>

#include <google/protobuf/message_lite.h>

#include <message_handler.h>

namespace lt {

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

protected:
    virtual bool initKeyMouse() = 0;
    virtual void onMouseEvent(const std::shared_ptr<google::protobuf::MessageLite>&) = 0;
    virtual void onKeyboardEvent(const std::shared_ptr<google::protobuf::MessageLite>&) = 0;

    void sendMessagte(uint32_t, const std::shared_ptr<google::protobuf::MessageLite>& msg);
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
    bool is_absolute_mouse_ = false;
    // 需要添加第三方库vigem才能使用gamepad
    // std::unique_ptr<Gamepad> gamepad_;
};

} // namespace lt
