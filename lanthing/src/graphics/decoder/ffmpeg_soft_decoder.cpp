#include "ffmpeg_soft_decoder.h"

#include <g3log/g3log.hpp>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/hwcontext.h>
} // extern "C"

namespace {

AVCodecID toAVCodecID(lt::VideoCodecType type) {
    switch (type) {
    case lt::VideoCodecType::H264:
        return AVCodecID::AV_CODEC_ID_H264;
    case lt::VideoCodecType::H265:
        return AVCodecID::AV_CODEC_ID_HEVC;
    default:
        return AVCodecID::AV_CODEC_ID_NONE;
    }
}

void* createTexture(void* hw_ctx, uint32_t width, uint32_t height) {
    (void)hw_ctx;
    (void)width;
    (void)height;
#if LT_WINDOWS
#elif LT_LINUX
#elif LT_ANDROID
#elif LT_MAC
#elif LT_IOS
#else
#error unknown platform
#endif
    return nullptr;
}

// TODO:
void releaseTexture(void* texture) {
    (void)texture;
#if LT_WINDOWS
#elif LT_LINUX
#elif LT_ANDROID
#elif LT_MAC
#elif LT_IOS
#else
#error unknown platform
#endif
}

void copyToTexture(AVFrame* frame, void* texture) {
    (void)frame;
    (void)texture;
}

void addRef(void* hw_ctx) {
    (void)hw_ctx;
}

void unRef(void* hw_ctx) {
    (void)hw_ctx;
}

} // namespace

namespace lt {

// 参考 https://ffmpeg.org/doxygen/4.4/decode__video_8c_source.html

std::unique_ptr<FFmpegSoftDecoder> FFmpegSoftDecoder::create(const Params& params) {
    std::unique_ptr<FFmpegSoftDecoder> decoder{new FFmpegSoftDecoder{params}};
    if (decoder->init()) {
        return nullptr;
    }
    return decoder;
}

FFmpegSoftDecoder::FFmpegSoftDecoder(const Params& params)
    : VideoDecoder{params}
    , hw_ctx_{params.hw_device} {}

FFmpegSoftDecoder::~FFmpegSoftDecoder() {
    if (codec_ctx_ != nullptr) {
        avcodec_free_context(reinterpret_cast<AVCodecContext**>(&codec_ctx_));
    }
    if (av_frame_ != nullptr) {
        av_frame_free(reinterpret_cast<AVFrame**>(&av_frame_));
    }
    if (av_packet_ != nullptr) {
        av_packet_free(reinterpret_cast<AVPacket**>(&av_packet_));
    }
    if (texture_ != nullptr) {
        releaseTexture(texture_);
    }
    if (hw_ctx_ != nullptr) {
        unRef(hw_ctx_);
    }
}

bool FFmpegSoftDecoder::init() {
    if (hw_ctx_ == nullptr) {
        LOG(WARNING) << "Create FFmpegSoftDecoder without hardware context";
        return false;
    }
    addRef(hw_ctx_);
    texture_ = createTexture(hw_ctx_, width(), height());
    if (texture_ == nullptr) {
        LOG(WARNING) << "Create texture for xxx failed";
        return false;
    }
    AVCodecID codec_id = toAVCodecID(codecType());
    if (codec_id == AVCodecID::AV_CODEC_ID_NONE) {
        LOG(FATAL) << "Unknown VideoCodecType " << (int)codecType();
        return false;
    }
    const AVCodec* codec = avcodec_find_decoder(codec_id);
    if (codec == nullptr) {
        LOGF(WARNING,
             "avcodec_find_decoder(%d) failed, maybe built libavcodec with wrong parameters",
             (int)codec_id);
        return false;
    }
    AVCodecContext* codec_ctx = avcodec_alloc_context3(codec);
    if (codec_ctx == nullptr) {
        LOGF(WARNING, "avcodec_alloc_context3(%s) failed", codec->name);
        return false;
    }
    codec_ctx_ = codec_ctx;
    constexpr size_t kBuffLen = 1024;
    char strbuff[kBuffLen] = {0};
    int ret = avcodec_open2(codec_ctx, codec, nullptr);
    if (ret != 0) {
        ret = av_strerror(ret, strbuff, kBuffLen);
        LOG(WARNING) << "avcodec_open2() failed: " << (ret == 0 ? strbuff : "unknown error");
        return false;
    }
    av_frame_ = av_frame_alloc();
    if (av_frame_ == nullptr) {
        LOG(WARNING) << "av_frame_alloc() failed";
        return false;
    }
    av_packet_ = av_packet_alloc();
    if (av_packet_ == nullptr) {
        LOG(WARNING) << "av_packet_alloc() failed";
        return false;
    }
    return true;
}

DecodedFrame FFmpegSoftDecoder::decode(const uint8_t* data, uint32_t size) {
    DecodedFrame frame{};
    constexpr size_t kBuffLen = 1024;
    char strbuff[kBuffLen] = {0};
    auto ctx = reinterpret_cast<AVCodecContext*>(codec_ctx_);
    auto packet = reinterpret_cast<AVPacket*>(av_packet_);
    packet->data = const_cast<uint8_t*>(data);
    packet->size = static_cast<int>(size);
    int ret = avcodec_send_packet(ctx, packet);
    if (ret == 0) {
        ;
    }
    else if (ret == AVERROR(EAGAIN)) {
        // 内部buffer满了，要先消费一部分解码完成，理论上不会走到这个分支
        frame.status = DecodeStatus::EAgain;
        return frame;
    }
    else {
        ret = av_strerror(ret, strbuff, kBuffLen);
        LOG(WARNING) << "avcodec_send_packet failed: " << (ret == 0 ? strbuff : "unknown error");
        frame.status = DecodeStatus::Failed;
        return frame;
    }

    auto av_frame = reinterpret_cast<AVFrame*>(av_frame_);
    ret = avcodec_receive_frame(ctx, av_frame);
    if (ret == 0) {
        copyToTexture(av_frame, texture_);
        frame.data = texture_;
        frame.status = DecodeStatus::Success;
        return frame;
    }
    else if (ret == AVERROR(EAGAIN)) {
        frame.status = DecodeStatus::EAgain;
        return frame;
    }
    else {
        frame.status = DecodeStatus::Failed;
        return frame;
    }
}

} // namespace lt