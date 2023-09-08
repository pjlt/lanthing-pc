#pragma once
#include <graphics/decoder/video_decoder.h>

#include <memory>

namespace lt {

class FFmpegSoftDecoder : public VideoDecoder {
public:
    static std::unique_ptr<FFmpegSoftDecoder> create(const Params& params);
    ~FFmpegSoftDecoder() override;

    DecodedFrame decode(const uint8_t* data, uint32_t size) override;

private:
    FFmpegSoftDecoder(const Params& params);
    bool init();

private:
    void* codec_ctx_ = nullptr;
    void* av_frame_ = nullptr;
    void* av_packet_ = nullptr;
    // D3D11下这个是IDevice11Device
    void* hw_ctx_ = nullptr;
    void* texture_ = nullptr;
};

} // namespace lt