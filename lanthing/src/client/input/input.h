#pragma once
#include <memory>
#include <client/input/input_event.h>

namespace lt
{

namespace cli
{

class PcSdl;
class InputImpl;

class Input
{
public:
    struct Params
    {
        PcSdl* sdl;
        uint32_t host_width;
        uint32_t host_height;
        std::function<void(uint32_t, const std::shared_ptr<google::protobuf::MessageLite>&, bool)> send_message;
    };
public:
    static std::unique_ptr<Input> create(const Params& params);

private:
    Input() = default;

private:
    std::shared_ptr<InputImpl> impl_;
};

} // namespace cli

} // namespace lt