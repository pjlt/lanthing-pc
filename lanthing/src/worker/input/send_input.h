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
    void onEvent(const std::shared_ptr<ltproto::peer2peer::MouseClick>&) override;
    void onEvent(const std::shared_ptr<ltproto::peer2peer::MouseMotion>&) override;
    void onEvent(const std::shared_ptr<ltproto::peer2peer::MouseWheel>&) override;
    void onEvent(const std::shared_ptr<ltproto::peer2peer::KeyboardEvent>&) override;

private:
    uint32_t screen_width_;
    uint32_t screen_height_;
};
} // namespace worker
} // namespace lt
