#pragma once
#include <graphics/decoder/video_decoder.h>

#include <list>
#include <memory>

#include <graphics/types.h>

namespace lt {

class FFmpegHardDecoder : public VideoDecoder {
public:
    FFmpegHardDecoder(const Params& params);
    ~FFmpegHardDecoder() override;

    bool init();
    DecodedFrame decode(const uint8_t* data, uint32_t size) override;
    std::vector<void*> textures() override;
    int32_t getHwPixFormat() const;
    void* getHwFrameCtx();

private:
    bool init2(const void* config, const void* codec);
    bool allocatePacketAndFrames();

private:
    void* hw_dev_;
    void* hw_ctx_;
    const VaType va_type_;
    void* codec_ctx_ = nullptr;
    void* av_packet_ = nullptr;
    void* av_frame_ = nullptr;
    void* hw_frames_ctx_ = nullptr;
    void* av_hw_ctx_ = nullptr;
    int32_t hw_pix_format_ = -1;
    std::vector<void*> textures_;
};

} // namespace lt