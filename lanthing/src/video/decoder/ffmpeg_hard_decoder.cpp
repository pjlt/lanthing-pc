/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2023 Zhennan Tu <zhennan.tu@gmail.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <cassert>

// ffmpeg头文件的警告
#include <ltlib/pragma_warning.h>
WARNING_DISABLE(4244)
#include "ffmpeg_hard_decoder.h"

#if LT_WINDOWS
#elif LT_LINUX
#include <va/va_drm.h>
#endif

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/hwcontext.h>
} // extern "C"
#if LT_WINDOWS
#include <libavutil/hwcontext_d3d11va.h>
#elif LT_LINUX
#include <libavutil/hwcontext_vaapi.h>
#else
#endif

#include <ltlib/logging.h>

WARNING_ENABLE(4244)

namespace {

AVHWDeviceType toAVHWDeviceType(lt::VaType type) {
    switch (type) {
    case lt::VaType::D3D11:
        return AVHWDeviceType::AV_HWDEVICE_TYPE_D3D11VA;
    case lt::VaType::VAAPI:
        return AVHWDeviceType::AV_HWDEVICE_TYPE_VAAPI;
    case lt::VaType::VTB:
        return AVHWDeviceType::AV_HWDEVICE_TYPE_VIDEOTOOLBOX;
    default:
        return AVHWDeviceType::AV_HWDEVICE_TYPE_NONE;
    }
}

AVCodecID toAVCodecID(lt::VideoCodecType type) {
    switch (type) {
    case lt::VideoCodecType::H264_420:
    case lt::VideoCodecType::H264_444:
        return AVCodecID::AV_CODEC_ID_H264;
    case lt::VideoCodecType::H265_420:
    case lt::VideoCodecType::H265_444:
        return AVCodecID::AV_CODEC_ID_HEVC;
    default:
        return AVCodecID::AV_CODEC_ID_NONE;
    }
}

void* getTexture(AVFrame* av_frame) {
#if LT_WINDOWS
    return av_frame->data[1];
#elif LT_LINUX
    return av_frame->data[3];
#else
    (void)av_frame;
    return nullptr;
#endif
}

AVPixelFormat getFormat(AVCodecContext* context, const enum AVPixelFormat* pixfmt) {
    // AVPixelFormat在不同上下文了似乎有不同语义
    auto that = reinterpret_cast<lt::video::FFmpegHardDecoder*>(context->opaque);
    auto format = static_cast<AVPixelFormat>(that->getHwPixFormat());
    for (auto p = pixfmt; *p != AV_PIX_FMT_NONE; p++) {
        if (*p == format) {
            context->hw_frames_ctx =
                av_buffer_ref(reinterpret_cast<AVBufferRef*>(that->getHwFrameCtx()));
            return format;
        }
    }
    return AVPixelFormat::AV_PIX_FMT_NONE;
}

void configAVHWDeviceContext(void* _avhw_dev_ctx, void* dev, void* ctx) {
    auto avhw_dev_ctx = reinterpret_cast<AVHWDeviceContext*>(_avhw_dev_ctx);
#if LT_WINDOWS
    auto av_d3d11_ctx = reinterpret_cast<AVD3D11VADeviceContext*>(avhw_dev_ctx->hwctx);
    av_d3d11_ctx->device = reinterpret_cast<ID3D11Device*>(dev);
    av_d3d11_ctx->device_context = reinterpret_cast<ID3D11DeviceContext*>(ctx);
    av_d3d11_ctx->device->AddRef();
    av_d3d11_ctx->device_context->AddRef();
#elif LT_LINUX
    // TODO: linux
    auto av_va_ctx = reinterpret_cast<AVVAAPIDeviceContext*>(avhw_dev_ctx->hwctx);
    av_va_ctx->display = ctx;
    (void)dev;
#else
    (void)avhw_dev_ctx;
    (void)dev;
    (void)ctx;
#endif
}

void configAVHWFramesContext(AVHWFramesContext* ctx) {
#if LT_WINDOWS
    auto d3d11_frames_ctx = reinterpret_cast<AVD3D11VAFramesContext*>(ctx->hwctx);
    // 就是为了这个D3D11_BIND_SHADER_RESOURCE才需要自定义AVHWFramesContext
    // TODO: 尝试向texture字段赋值，只使用一个texture
    d3d11_frames_ctx->BindFlags = D3D11_BIND_DECODER | D3D11_BIND_SHADER_RESOURCE;
#else
    // TODO: linux
    (void)ctx;
#endif
}

AVPixelFormat hwPixFormat() {
#if LT_WINDOWS
    return AVPixelFormat::AV_PIX_FMT_D3D11;
#elif LT_LINUX
    return AVPixelFormat::AV_PIX_FMT_VAAPI;
#elif LT_MAC
    return AVPixelFormat::AV_PIX_FMT_VIDEOTOOLBOX;
#else
    return AVPixelFormat::;
#endif
}

std::vector<void*> getTexturesFromAVHWFramesContext(AVHWFramesContext* ctx) {
    std::vector<void*> textures;
#if LT_WINDOWS
    auto d3d_ctx = reinterpret_cast<AVD3D11VAFramesContext*>(ctx->hwctx);
    textures.resize(ctx->initial_pool_size);
    for (int i = 0; i < ctx->initial_pool_size; i++) {
        textures[i] = d3d_ctx->texture_infos[i].texture;
    }
#else
    // TODO: linux
    (void)ctx;
#endif
    return textures;
}

} // namespace

namespace lt {

namespace video {

FFmpegHardDecoder::~FFmpegHardDecoder() {
    if (av_packet_ != nullptr) {
        av_packet_free(reinterpret_cast<AVPacket**>(&av_packet_));
    }
    if (av_frame_ != nullptr) {
        av_frame_free(reinterpret_cast<AVFrame**>(&av_frame_));
    }
    if (codec_ctx_ != nullptr) {
        avcodec_free_context(reinterpret_cast<AVCodecContext**>(&codec_ctx_));
    }
    if (hw_frames_ctx_ != nullptr) {
        av_buffer_unref(reinterpret_cast<AVBufferRef**>(&hw_frames_ctx_));
    }
    if (av_hw_ctx_ != nullptr) {
        av_buffer_unref(reinterpret_cast<AVBufferRef**>(&av_hw_ctx_));
    }
    deRefHwDevCtx();
}

FFmpegHardDecoder::FFmpegHardDecoder(const Params& params)
    : Decoder{params}
    , hw_dev_{params.hw_device}
    , hw_ctx_{params.hw_context}
    , va_type_{params.va_type} {}

bool FFmpegHardDecoder::init() {
    if (!addRefHwDevCtx()) {
        return false;
    }
    // 参考 https://www.ffmpeg.org/doxygen/4.4/hw__decode_8c_source.html
    //  由于我们已经提前知道编码类型，不需要像例子里那样，从码流中分析出AVCodec
    if (!allocatePacketAndFrames()) {
        return false;
    }
    AVHWDeviceType hw_type = toAVHWDeviceType(va_type_);
    if (hw_type == AVHWDeviceType::AV_HWDEVICE_TYPE_NONE) {
        LOG(FATAL) << "Unknown VaType " << (int)va_type_;
        return false;
    }
    AVCodecID codec_id = toAVCodecID(codecType());
    if (codec_id == AVCodecID::AV_CODEC_ID_NONE) {
        LOG(FATAL) << "Unknown VideoCodecType " << (int)codecType();
        return false;
    }
    const AVCodec* codec = avcodec_find_decoder(codec_id);
    if (codec == nullptr) {
        LOGF(ERR, "avcodec_find_decoder(%d) failed, maybe built libavcodec with wrong parameters",
             (int)codec_id);
        return false;
    }
    for (int i = 0;; i++) {
        const AVCodecHWConfig* config = avcodec_get_hw_config(codec, i);
        if (config == nullptr) {
            LOGF(ERR, "Decoder %s does not support device type %s", codec->name,
                 av_hwdevice_get_type_name(hw_type));
            return false;
        }
        // 本来只判断device_type就够了，但是ffmpeg内部有d3d11va和d3d11va2之分，我们要用的是d3d11va2
        // 前者的methods没有设置AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX，后者设置了（待确认）
        if (config->device_type != hw_type ||
            !(config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX)) {
            continue;
        }
        if (!init2(config, codec)) {
            continue;
        }
        return true;
    }
}

bool FFmpegHardDecoder::init2(const void* _config, const void* _codec) {
    auto config = reinterpret_cast<const AVCodecHWConfig*>(_config);
    auto codec = reinterpret_cast<const AVCodec*>(_codec);
    constexpr size_t kBuffLen = 1024;
    char strbuff[kBuffLen] = {0};
    AVCodecContext* codec_ctx = nullptr;
    AVBufferRef* avbuffref_hw_frames_ctx = nullptr;
    AVHWFramesContext* hw_frames_ctx = nullptr;
    int ret = 0;

    // 1
    // 将已有的设备上下文传入ffmpeg，不然不同D3D11设备之间需要'共享'资源，其他硬件加速api暂时不清楚
    // https://www.cnblogs.com/judgeou/p/14728617.html 中就没有使用同一个d3d11device
    // 导致多了一步OpenSharedResource
    AVBufferRef* avbuffref_hw_ctx = av_hwdevice_ctx_alloc(config->device_type);
    if (avbuffref_hw_ctx == nullptr) {
        LOG(ERR) << "av_hwdevice_ctx_alloc failed";
        goto INIT2_FAILED;
    }
    configAVHWDeviceContext(avbuffref_hw_ctx->data, hw_dev_, hw_ctx_);
    ret = av_hwdevice_ctx_init(avbuffref_hw_ctx);
    if (ret != 0) {
        ret = av_strerror(ret, strbuff, kBuffLen);
        LOG(ERR) << "av_hwdevice_ctx_init() failed: " << (ret == 0 ? strbuff : "unknown error");
        goto INIT2_FAILED;
    }
    // 2
    // 官方例子并没有创建AVHWFramesContext，但我们希望解码出来的在D3D11下能作为Shader Resource使用
    // 所以自定义AVHWFramesContext
    // TODO: 在非D3D11下能不能保留这个逻辑?
    avbuffref_hw_frames_ctx = av_hwframe_ctx_alloc(avbuffref_hw_ctx);
    if (avbuffref_hw_frames_ctx == nullptr) {
        LOG(ERR) << "av_hwframe_ctx_alloc failed";
        goto INIT2_FAILED;
    }
    hw_frames_ctx = reinterpret_cast<AVHWFramesContext*>(avbuffref_hw_frames_ctx->data);
    hw_frames_ctx->format = hwPixFormat();
    hw_frames_ctx->sw_format = AVPixelFormat::AV_PIX_FMT_NV12;
    hw_frames_ctx->width = FFALIGN(width(), align(codecType()));
    hw_frames_ctx->height = FFALIGN(height(), align(codecType()));
    hw_frames_ctx->initial_pool_size = 10;
    configAVHWFramesContext(hw_frames_ctx);
    ret = av_hwframe_ctx_init(avbuffref_hw_frames_ctx);
    if (ret != 0) {
        ret = av_strerror(ret, strbuff, kBuffLen);
        LOG(ERR) << "av_hwframe_ctx_init() failed: " << (ret == 0 ? strbuff : "unknown error");
        goto INIT2_FAILED;
    }
    textures_ = getTexturesFromAVHWFramesContext(hw_frames_ctx);

    // 3
    codec_ctx = avcodec_alloc_context3(codec);
    if (codec_ctx == nullptr) {
        LOGF(ERR, "avcodec_alloc_context3(%s) failed", codec->name);
        return false;
    }
    // codec_ctx->hw_device_ctx = avbuffref_hw_ctx; // 会被忽略
    codec_ctx->get_format = getFormat;
    codec_ctx->pix_fmt = AVPixelFormat::AV_PIX_FMT_YUV420P;
    codec_ctx->opaque = this;
    codec_ctx->width = width();
    codec_ctx->height = height();
    hw_pix_format_ = config->pix_fmt;
    ret = avcodec_open2(codec_ctx, codec, nullptr);
    if (ret != 0) {
        ret = av_strerror(ret, strbuff, kBuffLen);
        LOG(ERR) << "avcodec_open2() failed: " << (ret == 0 ? strbuff : "unknown error");
        goto INIT2_FAILED;
    }
    codec_ctx_ = codec_ctx;
    codec_ctx = nullptr;
    av_hw_ctx_ = avbuffref_hw_ctx;
    avbuffref_hw_ctx = nullptr;
    hw_frames_ctx_ = avbuffref_hw_frames_ctx;
    avbuffref_hw_frames_ctx = nullptr;
    return true;

INIT2_FAILED:
    if (codec_ctx != nullptr) {
        avcodec_free_context(&codec_ctx);
    }
    if (avbuffref_hw_ctx != nullptr) {
        av_buffer_unref(&avbuffref_hw_ctx);
    }
    if (avbuffref_hw_frames_ctx != nullptr) {
        av_buffer_unref(&avbuffref_hw_frames_ctx);
    }
    return false;
}

bool FFmpegHardDecoder::allocatePacketAndFrames() {
    av_packet_ = av_packet_alloc();
    if (av_packet_ == nullptr) {
        LOG(ERR) << "av_packet_alloc failed";
        return false;
    }
    av_frame_ = av_frame_alloc();
    if (av_frame_ == nullptr) {
        LOG(ERR) << "av_frame_alloc failed";
        return false;
    }
    return true;
}

bool FFmpegHardDecoder::addRefHwDevCtx() {
#ifdef LT_WINDOWS
    if (hw_ctx_ == nullptr || hw_dev_ == nullptr) {
        LOG(ERR) << "hw_ctx == nullptr or hw_dev == nullptr";
        return false;
    }
    else {
        auto ctx = reinterpret_cast<ID3D11DeviceContext*>(hw_ctx_);
        auto dev = reinterpret_cast<ID3D11Device*>(hw_dev_);
        ctx->AddRef();
        dev->AddRef();
        return true;
    }
#else
    return true;

#endif // LT_WINDOWS
}

void FFmpegHardDecoder::deRefHwDevCtx() {
#ifdef LT_WINDOWS
    if (hw_ctx_ == nullptr || hw_dev_ == nullptr) {
        return;
    }
    auto ctx = reinterpret_cast<ID3D11DeviceContext*>(hw_ctx_);
    auto dev = reinterpret_cast<ID3D11Device*>(hw_dev_);
    ctx->Release();
    dev->Release();
#endif // LT_WINDOWS
}

int32_t FFmpegHardDecoder::getHwPixFormat() const {
    return hw_pix_format_;
}

void* FFmpegHardDecoder::getHwFrameCtx() {
    return hw_frames_ctx_;
}

DecodedFrame FFmpegHardDecoder::decode(const uint8_t* data, uint32_t size) {
    constexpr size_t kBuffLen = 1024;
    char strbuff[kBuffLen] = {0};
    auto ctx = reinterpret_cast<AVCodecContext*>(codec_ctx_);
    auto packet = reinterpret_cast<AVPacket*>(av_packet_);
    packet->data = const_cast<uint8_t*>(data);
    packet->size = static_cast<int>(size);
    DecodedFrame frame{};
    int ret = avcodec_send_packet(ctx, packet);
    if (ret == 0) {
        ;
    }
    else if (ret == AVERROR(EAGAIN)) {
        frame.status = DecodeStatus::EAgain;
        return frame;
    }
    else if (ret == AVERROR(EPERM)) {
        frame.status = DecodeStatus::NeedReset;
        return frame;
    }
    else {
        ret = av_strerror(ret, strbuff, kBuffLen);
        LOG(ERR) << "avcodec_send_packet failed: " << (ret == 0 ? strbuff : "unknown error");
        frame.status = DecodeStatus::Failed;
        return frame;
    }

    // 原本以为avcodec_send_packet()会是异步的，设计了轮询模型，但是ffmpeg内部似乎会把一些异步的解码器封装成同步的
    // 比方说对英特尔解码器的封装：https://www.ffmpeg.org/doxygen/4.4/libavcodec_2qsvdec_8c_source.html#l00444
    // 又改回avcodec_send_packet之后马上avcodec_receive_frame
    auto av_frame = reinterpret_cast<AVFrame*>(av_frame_);
    ret = avcodec_receive_frame(ctx, av_frame);
    if (ret == 0) {
        frame.frame = static_cast<int64_t>((uintptr_t)getTexture(av_frame));
        frame.status = DecodeStatus::Success2;
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

std::vector<void*> FFmpegHardDecoder::textures() {
    return textures_;
}

DecodedFormat FFmpegHardDecoder::decodedFormat() const {
    switch (va_type_) {
    case VaType::D3D11:
        return DecodedFormat::D3D11_NV12;
    case VaType::VAAPI:
        return DecodedFormat::VA_NV12;
    default:
        // client 在初始化阶段用FATAL没问题
        LOG(FATAL) << "Unknown VaType " << (int)va_type_;
        return DecodedFormat::D3D11_NV12;
    }
}

} // namespace video

} // namespace lt