#pragma once

#include "input.h"

namespace lt {
namespace worker {
class Win32SendInput : public Input {
public:
    Win32SendInput(uint32_t screen_width, uint32_t screen_height)
        : screen_width_(screen_width)
        , screen_height_(screen_height) {}

private:
    bool initKeyMouse() override;
    void onMouseEvent(const std::shared_ptr<google::protobuf::MessageLite>&) override;
    void onKeyboardEvent(const std::shared_ptr<google::protobuf::MessageLite>&) override;

private:
    uint32_t screen_width_;
    uint32_t screen_height_;
};
} // namespace worker
} // namespace lt
