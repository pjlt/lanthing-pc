#pragma once
#include <cstdint>
#include <functional>
#include <future>
#include <memory>

#include <inputs/capturer/input_event.h>

extern "C" {

struct SDL_Window;

} // extern "C"

namespace lt {

class PcSdl {
public:
    struct Params {
        std::function<void()> on_reset;
        std::function<void()> on_exit;
    };

public:
    static std::unique_ptr<PcSdl> create(const Params& params);
    virtual ~PcSdl(){};
    // virtual void set_negotiated_params(uint32_t width, uint32_t height) = 0;
    virtual SDL_Window* window() = 0;

    virtual void setInputHandler(const OnInputEvent&) = 0;

protected:
    PcSdl() = default;
};

} // namespace lt