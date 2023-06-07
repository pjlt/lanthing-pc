#pragma once
#include <cstdint>
#include <functional>
#include <future>
#include <memory>

#include "client/input/input_event.h"

extern "C" {

struct SDL_Window;

} // extern "C"

namespace lt {

namespace cli {

class PcSdl {
public:
    struct Params {
        uint32_t window_width = 0;
        uint32_t window_height = 0;
        uint32_t video_width = 0;
        uint32_t video_height = 0;
        std::function<void()> on_reset;
        std::function<void()> on_exit;
    };

public:
    static std::unique_ptr<PcSdl> create(const Params& params);
    virtual ~PcSdl(){};
    // virtual void set_negotiated_params(uint32_t width, uint32_t height) = 0;
    virtual SDL_Window* window() = 0;

    virtual void set_input_handler(const OnInputEvent&) = 0;

protected:
    PcSdl() = default;
};

} // namespace cli

} // namespace lt