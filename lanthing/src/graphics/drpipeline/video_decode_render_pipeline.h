#pragma once
#include <cstdint>
#include <functional>
#include <memory>

#include <google/protobuf/message_lite.h>

#include <platforms/pc_sdl.h>
#include <transport/transport.h>

namespace lt {

class VDRPipeline;
class VideoDecodeRenderPipeline {
public:
    struct Params {
        Params(lt::VideoCodecType _codec_type, uint32_t _width, uint32_t _height,
               uint32_t _screen_refresh_rate,
               std::function<void(uint32_t, std::shared_ptr<google::protobuf::MessageLite>, bool)>
                   send_message);
        bool validate() const;

        lt::VideoCodecType codec_type;
        uint32_t width;
        uint32_t height;
        uint32_t screen_refresh_rate;
        PcSdl* sdl = nullptr;
        std::function<void(uint32_t, std::shared_ptr<google::protobuf::MessageLite>, bool)>
            send_message_to_host;
    };

    enum class Action {
        REQUEST_KEY_FRAME = 1,
        NONE = 2,
    };

public:
    static std::unique_ptr<VideoDecodeRenderPipeline> create(const Params& params);
    void reset();
    Action submit(const lt::VideoFrame& frame);

private:
    VideoDecodeRenderPipeline() = default;

private:
    std::shared_ptr<VDRPipeline> impl_;
};

} // namespace lt