#pragma once

#include <cstdint>
#include <memory>

#include <ltproto/peer2peer/keyboard_event.pb.h>
#include <ltproto/peer2peer/mouse_click.pb.h>
#include <ltproto/peer2peer/mouse_motion.pb.h>
#include <ltproto/peer2peer/mouse_wheel.pb.h>

namespace lt {

namespace worker {

class Input {
public:
    enum class Type : uint8_t {
        WIN32_MESSAGE = 1,
        WIN32_DRIVER = 2,
    };
    struct Params {
        uint8_t types = 0;
        uint32_t screen_width = 0;
        uint32_t screen_height = 0;
    };
    static std::unique_ptr<Input> create(const Params& params);

public:
    virtual ~Input() = default;

    // NOTE: 直接使用proto作为参数传递
    virtual void onEvent(const std::shared_ptr<ltproto::peer2peer::MouseClick>&) = 0;
    virtual void onEvent(const std::shared_ptr<ltproto::peer2peer::MouseMotion>&) = 0;
    virtual void onEvent(const std::shared_ptr<ltproto::peer2peer::MouseWheel>&) = 0;
    virtual void onEvent(const std::shared_ptr<ltproto::peer2peer::KeyboardEvent>&) = 0;
};

} // namespace worker

} // namespace lt
