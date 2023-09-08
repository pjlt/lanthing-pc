#include "video_decoder.h"

#include "ffmpeg_hard_decoder.h"

namespace lt {

std::unique_ptr<VideoDecoder> VideoDecoder::create(const Params& params) {
    auto decoder = std::make_unique<FFmpegHardDecoder>(params);
    if (!decoder->init()) {
        return nullptr;
    }
    return decoder;
}

VideoDecoder::VideoDecoder(const Params& params)
    : codec_type_{params.codec_type}
    , width_{params.width}
    , height_{params.height} {}

VideoCodecType VideoDecoder::codecType() const {
    return codec_type_;
}

uint32_t VideoDecoder::width() const {
    return width_;
}

uint32_t VideoDecoder::height() const {
    return height_;
}

} // namespace lt