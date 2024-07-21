/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2024 Zhennan Tu <zhennan.tu@gmail.com>
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

#include "openh264_encoder.h"

#include <wels/codec_api.h>

#include <ltlib/load_library.h>
#include <ltlib/logging.h>
#include <ltlib/times.h>

namespace {

constexpr int kMaxFPS = 30;

class OpenH264ParamsHelper {
public:
    OpenH264ParamsHelper(const lt::video::EncodeParamsHelper& params)
        : params_{params} {}

    int fps() const { return std::min(params_.fps(), kMaxFPS); }
    uint32_t width() const { return params_.width(); }
    uint32_t height() const { return params_.height(); }
    uint32_t bitrate() const { return params_.bitrate(); }
    uint32_t maxbitrate() const { return params_.maxbitrate(); }
    void set_bitrate(uint32_t bps) { params_.set_bitrate(bps); }
    void set_fps(int f) { params_.set_fps(f); }

private:
    lt::video::EncodeParamsHelper params_;
};
} // namespace

namespace lt {

namespace video {

class OpenH264EncoderImpl {
public:
    OpenH264EncoderImpl(const EncodeParamsHelper& params);
    ~OpenH264EncoderImpl();
    bool init();
    void reconfigure(const Encoder::ReconfigureParams& params);
    uint32_t width() const { return params_.width(); }
    uint32_t height() const { return params_.height(); }
    std::shared_ptr<ltproto::client2worker::VideoFrame> encodeOneFrame(void* input_frame,
                                                                       bool request_iframe);

private:
    bool loadApi();
    void generateEncodeParams(const OpenH264ParamsHelper& helper, SEncParamExt& params);

private:
    VideoCodecType codec_type_;
    std::unique_ptr<ltlib::DynamicLibrary> openh264_lib_;
    ISVCEncoder* encoder_ = nullptr;
    SEncParamExt init_params_{};
    decltype(&WelsCreateSVCEncoder) create_encoder_ = nullptr;
    decltype(&WelsDestroySVCEncoder) destroy_encoder_ = nullptr;
    bool encoder_init_success_ = false;
    OpenH264ParamsHelper params_;
};

OpenH264EncoderImpl::OpenH264EncoderImpl(const EncodeParamsHelper& params)
    : codec_type_{params.codec()}
    , params_{params} {}

OpenH264EncoderImpl::~OpenH264EncoderImpl() {
    if (encoder_ != nullptr) {
        if (encoder_init_success_) {
            encoder_->Uninitialize();
        }
        destroy_encoder_(encoder_);
    }
}

bool OpenH264EncoderImpl::init() {
    if (codec_type_ != VideoCodecType::H264_420 && codec_type_ != VideoCodecType::H264_420_SOFT) {
        LOG(ERR) << "OpenH264 encoder only support H264_420";
        return false;
    }
    if (!loadApi()) {
        return false;
    }

    int ret = create_encoder_(&encoder_);
    if (ret != 0) {
        LOG(ERR) << "WelsCreateSVCEncoder failed " << ret;
        return false;
    }
    ret = encoder_->GetDefaultParams(&init_params_);
    if (ret != 0) {
        LOG(ERR) << "ISVCEncoder::GetDefaultParams failed " << ret;
        return false;
    }
    generateEncodeParams(params_, init_params_);
    ret = encoder_->InitializeExt(&init_params_);
    if (ret != 0) {
        LOG(ERR) << "ISVCEncoder::InitializeExt failed " << ret;
        return false;
    }
    encoder_init_success_ = true;
    int option = EVideoFormatType::videoFormatI420;
    ret = encoder_->SetOption(ENCODER_OPTION_DATAFORMAT, &option);
    if (ret != 0) {
        LOG(ERR) << "ISVCEncoder::SetOption(ENCODER_OPTION_DATAFORMAT, videoFormatI420) failed "
                 << ret;
        return false;
    }
    return true;
}

void OpenH264EncoderImpl::reconfigure(const Encoder::ReconfigureParams& params) {
    if (params.bitrate_bps.has_value()) {
        params_.set_bitrate(params.bitrate_bps.value());
        SBitrateInfo option{};
        option.iLayer = SPATIAL_LAYER_ALL;
        option.iBitrate = static_cast<int>(params_.bitrate());
        int ret = encoder_->SetOption(ENCODER_OPTION_BITRATE, &option);
        if (ret != 0) {
            LOG(ERR) << "ISVCEncoder::SetOption(ENCODER_OPTION_BITRATE, " << params_.bitrate()
                     << ") failed " << ret;
        }
    }
    if (params.fps.has_value()) {
        params_.set_fps(params.fps.value());
        float option = static_cast<float>(params_.fps());
        int ret = encoder_->SetOption(ENCODER_OPTION_FRAME_RATE, &option);
        if (ret != 0) {
            LOG(ERR) << "ISVCEncoder::SetOption(ENCODER_OPTION_FRAME_RATE, " << option
                     << ") failed " << ret;
        }
    }
}

std::shared_ptr<ltproto::client2worker::VideoFrame>
OpenH264EncoderImpl::encodeOneFrame(void* input_frame, bool request_iframe) {
    SSourcePicture src{};
    src.iColorFormat = EVideoFormatType::videoFormatI420;
    src.iPicHeight = init_params_.iPicHeight;
    src.iPicWidth = init_params_.iPicWidth;
    src.uiTimeStamp = ltlib::steady_now_ms();
    src.iStride[0] = src.iPicWidth;
    src.iStride[1] = src.iPicWidth / 2;
    src.iStride[2] = src.iPicWidth / 2;
    src.pData[0] = reinterpret_cast<uint8_t*>(input_frame);
    src.pData[1] = src.pData[0] + src.iPicWidth * src.iPicHeight;
    src.pData[2] = src.pData[1] + (src.iPicWidth * src.iPicHeight / 4);
    if (request_iframe) {
        int ret = encoder_->ForceIntraFrame(true);
        if (ret != 0) {
            LOG(ERR) << "ISVCEncoder::ForceIntraFrame failed " << ret;
        }
    }
    SFrameBSInfo info{};
    int ret = encoder_->EncodeFrame(&src, &info);
    if (ret != 0) {
        LOG(ERR) << "ISVCEncoder::EncodeFrame failed " << ret;
        return nullptr;
    }
    auto out_frame = std::make_shared<ltproto::client2worker::VideoFrame>();
    switch (info.eFrameType) {
    case EVideoFrameType::videoFrameTypeIDR:
    case EVideoFrameType::videoFrameTypeI:
        out_frame->set_is_keyframe(true);
        break;
    case EVideoFrameType::videoFrameTypeP:
        out_frame->set_is_keyframe(false);
        break;
    case EVideoFrameType::videoFrameTypeSkip:
        LOG(ERR) << "FATAL ERROR: ISVCEncoder::EncodeFrame done with 'videoFrameTypeSkip'";
        return nullptr;
    default:
        LOG(ERR) << "FATAL ERROR: ISVCEncoder::EncodeFrame done with unkown eFrameType "
                 << (int)info.eFrameType;
        return nullptr;
    }
    // credit: WebRTC
    size_t required_capacity = 0;
    size_t fragments_count = 0;
    for (int layer = 0; layer < info.iLayerNum; ++layer) {
        const SLayerBSInfo& layerInfo = info.sLayerInfo[layer];
        for (int nal = 0; nal < layerInfo.iNalCount; ++nal, ++fragments_count) {
            required_capacity += layerInfo.pNalLengthInByte[nal];
        }
    }
    std::vector<uint8_t> buff;
    buff.resize(required_capacity);
    size_t copied = 0;
    for (int layer = 0; layer < info.iLayerNum; ++layer) {
        const SLayerBSInfo& layerInfo = info.sLayerInfo[layer];
        // Iterate NAL units making up this layer, noting fragments.
        size_t layer_len = 0;
        for (int nal = 0; nal < layerInfo.iNalCount; ++nal) {
            layer_len += layerInfo.pNalLengthInByte[nal];
        }
        // Copy the entire layer's data (including start codes).
        memcpy(buff.data() + copied, layerInfo.pBsBuf, layer_len);
        copied += layer_len;
    }
    out_frame->set_frame(buff.data(), copied);
    return out_frame;
}

bool OpenH264EncoderImpl::loadApi() {
    const std::string kLibName = "openh264-2.4.0-win64.dll";
    openh264_lib_ = ltlib::DynamicLibrary::load(kLibName);
    if (openh264_lib_ == nullptr) {
        LOG(ERR) << "Load library " << kLibName << " failed";
        return false;
    }
    create_encoder_ = reinterpret_cast<decltype(&WelsCreateSVCEncoder)>(
        openh264_lib_->getFunc("WelsCreateSVCEncoder"));
    if (create_encoder_ == nullptr) {
        LOG(ERR) << "Load function WelsCreateSVCEncoder from " << kLibName << " failed";
        return false;
    }
    destroy_encoder_ = reinterpret_cast<decltype(&WelsDestroySVCEncoder)>(
        openh264_lib_->getFunc("WelsDestroySVCEncoder"));
    if (destroy_encoder_ == nullptr) {
        LOG(ERR) << "Load function WelsDestroySVCEncoder from " << kLibName << " failed";
        return false;
    }
    return true;
}

void OpenH264EncoderImpl::generateEncodeParams(const OpenH264ParamsHelper& helper,
                                               SEncParamExt& params) {
    params.iPicWidth = helper.width();
    params.iPicHeight = helper.height();
    params.fMaxFrameRate = helper.fps() * 1.f;
    params.iUsageType = CAMERA_VIDEO_REAL_TIME;
    params.iRCMode = RC_BITRATE_MODE;
    params.iTargetBitrate = helper.bitrate();
    params.iMaxBitrate = UNSPECIFIED_BIT_RATE;
    params.bEnableFrameSkip = false;
    params.uiIntraPeriod = 0;
    params.uiMaxNalSize = 0;
    params.iMultipleThreadIdc = 1;
    params.iTemporalLayerNum = 1;
    params.iNumRefFrame = 1;
    params.sSpatialLayers[0].iVideoWidth = params.iPicWidth;
    params.sSpatialLayers[0].iVideoHeight = params.iPicHeight;
    params.sSpatialLayers[0].fFrameRate = params.fMaxFrameRate;
    params.sSpatialLayers[0].iSpatialBitrate = params.iTargetBitrate;
    params.sSpatialLayers[0].iMaxSpatialBitrate = params.iMaxBitrate;
    params.sSpatialLayers[0].sSliceArgument.uiSliceNum = 1;
    params.sSpatialLayers[0].sSliceArgument.uiSliceMode = SM_SINGLE_SLICE;
}

std::unique_ptr<OpenH264Encoder> OpenH264Encoder::create(const EncodeParamsHelper& params) {
    auto encoder = std::make_unique<OpenH264Encoder>();
    auto impl = std::make_shared<OpenH264EncoderImpl>(params);
    if (!impl->init()) {
        return nullptr;
    }
    encoder->impl_ = impl;
    return encoder;
}

void OpenH264Encoder::reconfigure(const ReconfigureParams& params) {
    impl_->reconfigure(params);
}

CaptureFormat OpenH264Encoder::captureFormat() const {
    return CaptureFormat::MEM_I420;
}

VideoCodecType OpenH264Encoder::codecType() const {
    return VideoCodecType::H264_420_SOFT;
}

uint32_t OpenH264Encoder::width() const {
    return impl_->width();
}

uint32_t OpenH264Encoder::height() const {
    return impl_->height();
}

std::shared_ptr<ltproto::client2worker::VideoFrame>
OpenH264Encoder::encodeFrame(void* input_frame) {
    return impl_->encodeOneFrame(input_frame, needKeyframe());
}

} // namespace video

} // namespace lt
