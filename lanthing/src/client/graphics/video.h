#pragma once
#include <cstdint>
#include <functional>
#include <memory>
#include <google/protobuf/message_lite.h>
#include <ltrtc/lttypes.h>
#include <client/platforms/pc_sdl.h>

namespace lt
{

namespace cli
{

class VideoImpl;
class Video
{
public:
    struct Params
    {
        Params(ltrtc::VideoCodecType _codec_type, uint32_t _width, uint32_t _height, uint32_t _screen_refresh_rate, std::function<void(uint32_t, std::shared_ptr<google::protobuf::MessageLite>, bool)> send_message);
        bool validate() const;

        ltrtc::VideoCodecType codec_type;
        uint32_t width;
        uint32_t height;
        uint32_t screen_refresh_rate;
        PcSdl* sdl = nullptr;
        std::function<void(uint32_t, std::shared_ptr<google::protobuf::MessageLite>, bool)> send_message_to_host;
    };

    enum class Action
    {
        REQUEST_KEY_FRAME = 1,
        NONE = 2,
    };

public:
    static std::unique_ptr<Video> create(const Params& params);
    void reset_decoder_renderer();
    Action submit(const ltrtc::VideoFrame& frame);

private:
    Video() = default;

private:
    std::shared_ptr<VideoImpl> impl_;
};

} // namespace cli

} // namespace lt