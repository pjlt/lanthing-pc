#include "input.h"

#include "send_input.h"

namespace lt {
namespace worker {

std::unique_ptr<Input> Input::create(const Params& params) {
    std::unique_ptr<Input> input;
    if (params.types & static_cast<uint8_t>(Type::WIN32_MESSAGE)) {
        input = std::make_unique<Win32SendInput>(params.screen_width, params.screen_height);
    }

    return input;
};
} // namespace worker
} // namespace lt
