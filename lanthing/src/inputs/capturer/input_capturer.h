#pragma once
#include <inputs/capturer/input_event.h>

#include <memory>

#include <google/protobuf/api.pb.h>

namespace lt {

class PcSdl;
class InputCapturerImpl;

class InputCapturer {
public:
    struct Params {
        PcSdl* sdl;
        uint32_t host_width;
        uint32_t host_height;
        std::function<void(uint32_t, const std::shared_ptr<google::protobuf::MessageLite>&, bool)>
            send_message;
    };

public:
    static std::unique_ptr<InputCapturer> create(const Params& params);

private:
    InputCapturer() = default;

private:
    std::shared_ptr<InputCapturerImpl> impl_;
};

} // namespace lt