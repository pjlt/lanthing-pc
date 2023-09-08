#pragma once
#include <cstdint>
#include <memory>

#include <graphics/types.h>
#include <transport/transport.h>

namespace lt {

enum class DecodeStatus { Success, EAgain, Failed };

struct DecodedFrame {
    DecodeStatus status;
    int64_t frame;
};

class VideoDecoder {
public:
    struct Params {
        VideoCodecType codec_type;
        uint32_t width;
        uint32_t height;
        void* hw_device;
        void* hw_context;
        VaType va_type;
    };

public:
    static std::unique_ptr<VideoDecoder> create(const Params& params);
    VideoDecoder(const Params& params);
    virtual ~VideoDecoder() = default;
    virtual DecodedFrame decode(const uint8_t* data, uint32_t size) = 0;
    virtual std::vector<void*> textures() = 0;

    VideoCodecType codecType() const;
    uint32_t width() const;
    uint32_t height() const;

private:
    const VideoCodecType codec_type_;
    const uint32_t width_;
    const uint32_t height_;
};

} // namespace lt