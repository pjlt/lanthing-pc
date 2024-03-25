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

#include "amd_encoder.h"

#include <d3d11.h>
#include <wrl/client.h>

#include <amf/components/VideoEncoderHEVC.h>
#include <amf/components/VideoEncoderVCE.h>
#include <amf/core/Factory.h>

#include <ltlib/logging.h>

#include <ltlib/load_library.h>

namespace {

amf::AMF_SURFACE_FORMAT toAmfFormat(DXGI_FORMAT format) {
    // TODO:
    (void)format;
    return amf::AMF_SURFACE_BGRA;
}

class AmfParamsHelper {
public:
    AmfParamsHelper(const lt::video::EncodeParamsHelper& params)
        : params_{params} {}
    uint32_t width() const { return params_.width(); }
    uint32_t height() const { return params_.height(); }
    std::wstring codec() const;
    int fps() const { return params_.fps(); }
    int64_t gop() const { return params_.gop() < 0 ? 0 : params_.gop(); }
    int64_t bitrate() const { return params_.bitrate(); }
    int64_t max_bitrate() const { return params_.maxbitrate(); }
    int64_t qmin() const { return params_.qmin()[0]; }
    int64_t qmax() const { return params_.qmax()[0]; }
    AMF_VIDEO_ENCODER_RATE_CONTROL_METHOD_ENUM rc() const;
    AMF_VIDEO_ENCODER_QUALITY_PRESET_ENUM presetAvc() const;
    AMF_VIDEO_ENCODER_HEVC_QUALITY_PRESET_ENUM presetHevc() const;
    void set_bitrate(uint32_t bps) { params_.set_bitrate(bps); }
    void set_fps(int f) { params_.set_fps(f); }

private:
    lt::video::EncodeParamsHelper params_;
};

std::wstring AmfParamsHelper::codec() const {
    switch (params_.codec()) {
    case lt::VideoCodecType::H264:
        return AMFVideoEncoderVCE_AVC;
    case lt::VideoCodecType::H265:
        return AMFVideoEncoder_HEVC;
    default:
        assert(false);
        return L"Unknown";
    }
}

AMF_VIDEO_ENCODER_QUALITY_PRESET_ENUM AmfParamsHelper::presetAvc() const {
    switch (params_.preset()) {
    case lt::video::EncodeParamsHelper::Preset::Balanced:
        return AMF_VIDEO_ENCODER_QUALITY_PRESET_BALANCED;
    case lt::video::EncodeParamsHelper::Preset::Speed:
        return AMF_VIDEO_ENCODER_QUALITY_PRESET_SPEED;
    case lt::video::EncodeParamsHelper::Preset::Quality:
        return AMF_VIDEO_ENCODER_QUALITY_PRESET_QUALITY;
    default:
        assert(false);
        return AMF_VIDEO_ENCODER_QUALITY_PRESET_BALANCED;
    }
}

AMF_VIDEO_ENCODER_HEVC_QUALITY_PRESET_ENUM AmfParamsHelper::presetHevc() const {
    switch (params_.preset()) {
    case lt::video::EncodeParamsHelper::Preset::Balanced:
        return AMF_VIDEO_ENCODER_HEVC_QUALITY_PRESET_BALANCED;
    case lt::video::EncodeParamsHelper::Preset::Speed:
        return AMF_VIDEO_ENCODER_HEVC_QUALITY_PRESET_SPEED;
    case lt::video::EncodeParamsHelper::Preset::Quality:
        return AMF_VIDEO_ENCODER_HEVC_QUALITY_PRESET_QUALITY;
    default:
        assert(false);
        return AMF_VIDEO_ENCODER_HEVC_QUALITY_PRESET_BALANCED;
    }
}

AMF_VIDEO_ENCODER_RATE_CONTROL_METHOD_ENUM AmfParamsHelper::rc() const {
    switch (params_.rc()) {
    case lt::video::EncodeParamsHelper::RcMode::CBR:
        return AMF_VIDEO_ENCODER_RATE_CONTROL_METHOD_CBR;
    case lt::video::EncodeParamsHelper::RcMode::VBR:
        return AMF_VIDEO_ENCODER_RATE_CONTROL_METHOD_LATENCY_CONSTRAINED_VBR;
    default:
        assert(false);
        return AMF_VIDEO_ENCODER_RATE_CONTROL_METHOD_UNKNOWN;
    }
}

} // namespace

namespace lt {

namespace video {

class AmdEncoderImpl {
public:
    AmdEncoderImpl(const EncodeParamsHelper& params);
    ~AmdEncoderImpl();
    bool init();
    void reconfigure(const Encoder::ReconfigureParams& params);
    VideoCodecType codecType() const;
    uint32_t width() const { return params_.width(); }
    uint32_t height() const { return params_.height(); }
    std::shared_ptr<ltproto::client2worker::VideoFrame> encodeOneFrame(void* input_frame,
                                                                       bool request_iframe);

private:
    bool loadAmdApi();
    bool setAvcEncodeParams(const AmfParamsHelper& params);
    bool setHevcEncodeParams(const AmfParamsHelper& params);
    bool isKeyFrame(amf::AMFDataPtr data);

private:
    Microsoft::WRL::ComPtr<ID3D11Device> d3d11_dev_;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> d3d11_ctx_;

    lt::VideoCodecType codec_type_;
    std::unique_ptr<ltlib::DynamicLibrary> amdapi_;
    amf::AMFFactory* factory_ = nullptr;
    amf::AMFContextPtr context_ = nullptr;
    amf::AMFComponentPtr encoder_ = nullptr;
    AMF_RESULT last_submit_error_ = AMF_OK;
    AmfParamsHelper params_;
};

AmdEncoderImpl::AmdEncoderImpl(const EncodeParamsHelper& params)
    : d3d11_dev_{reinterpret_cast<ID3D11Device*>(params.d3d11_dev())}
    , d3d11_ctx_{reinterpret_cast<ID3D11DeviceContext*>(params.d3d11_ctx())}
    , codec_type_{params.codec()}
    , params_{params} {}

AmdEncoderImpl::~AmdEncoderImpl() {}

bool AmdEncoderImpl::init() {
    if (codec_type_ != lt::VideoCodecType::H264 && codec_type_ != lt::VideoCodecType::H265) {
        LOG(FATAL) << "Unknown video codec type " << (int)codec_type_;
        return false;
    }

    if (!loadAmdApi()) {
        return false;
    }
    AMF_RESULT result = factory_->CreateContext(&context_);
    if (result != AMF_OK) {
        LOG(ERR) << "AMFFactory::CreateContext failed with " << result;
        return false;
    }
    result = context_->InitDX11(d3d11_dev_.Get());
    if (result != AMF_OK) {
        LOG(ERR) << "AMFFactory::InitDX11 failed with " << result;
        return false;
    }
    result = factory_->CreateComponent(context_, params_.codec().c_str(), &encoder_);
    if (result != AMF_OK) {
        LOG(ERR) << "AMFFactory::CreateComponent failed with " << result;
        return false;
    }
    if (codec_type_ == lt::VideoCodecType::H264) {
        if (!setAvcEncodeParams(params_)) {
            return false;
        }
    }
    else {
        if (!setHevcEncodeParams(params_)) {
            return false;
        }
    }
    result = encoder_->Init(amf::AMF_SURFACE_BGRA, params_.width(), params_.height());
    if (result != AMF_OK) {
        LOG(ERR) << "AMFComponent::Init failed with " << result;
        return false;
    }
    return true;
}

void AmdEncoderImpl::reconfigure(const Encoder::ReconfigureParams& params) {
    // FIXME: 是否可以动态设置fps?
    AMF_RESULT result = AMF_OK;
    AMF_RESULT result2 = AMF_OK;
    if (params.bitrate_bps.has_value()) {
        params_.set_bitrate(params.bitrate_bps.value());
        if (codec_type_ == lt::VideoCodecType::H264_420) {
            result = encoder_->SetProperty(AMF_VIDEO_ENCODER_TARGET_BITRATE,
                                           static_cast<int64_t>(params_.bitrate()));
            result2 = encoder_->SetProperty(AMF_VIDEO_ENCODER_PEAK_BITRATE,
                                            static_cast<int64_t>(params_.max_bitrate()));
        }
        else if (codec_type_ == lt::VideoCodecType::H265_420) {
            result = encoder_->SetProperty(AMF_VIDEO_ENCODER_HEVC_TARGET_BITRATE,
                                           static_cast<int64_t>(params_.bitrate()));
            result2 = encoder_->SetProperty(AMF_VIDEO_ENCODER_HEVC_PEAK_BITRATE,
                                            static_cast<int64_t>(params_.max_bitrate()));
        }
        else {
            assert(false);
            return;
        }
    }
    if (result != AMF_OK) {
        LOG(ERR)
            << "Set AMF_VIDEO_ENCODER_TARGET_BITRATE/AMF_VIDEO_ENCODER_HEVC_TARGET_BITRATE failed "
               "with "
            << result;
    }
    if (result2 != AMF_OK) {
        LOG(ERR) << "Set AMF_VIDEO_ENCODER_PEAK_BITRATE/AMF_VIDEO_ENCODER_HEVC_PEAK_BITRATE failed "
                    "with "
                 << result2;
    }
}

VideoCodecType AmdEncoderImpl::codecType() const {
    return codec_type_;
}

std::shared_ptr<ltproto::client2worker::VideoFrame>
AmdEncoderImpl::encodeOneFrame(void* input_frame, bool request_iframe) {
    amf::AMFSurfacePtr surface = nullptr;
    AMF_RESULT result = context_->CreateSurfaceFromDX11Native(input_frame, &surface, nullptr);
    if (result != AMF_OK) {
        LOG(ERR) << "AMFContext::CreateSurfaceFromDX11Native failed with " << result;
        return nullptr;
    }
    if (request_iframe) {
        if (codec_type_ == lt::VideoCodecType::H264) {
            result = surface->SetProperty(
                AMF_VIDEO_ENCODER_FORCE_PICTURE_TYPE,
                AMF_VIDEO_ENCODER_PICTURE_TYPE_ENUM::AMF_VIDEO_ENCODER_PICTURE_TYPE_IDR);
        }
        else if (codec_type_ == lt::VideoCodecType::H265) {
            result = surface->SetProperty(
                AMF_VIDEO_ENCODER_HEVC_FORCE_PICTURE_TYPE,
                AMF_VIDEO_ENCODER_HEVC_PICTURE_TYPE_ENUM::AMF_VIDEO_ENCODER_HEVC_PICTURE_TYPE_IDR);
        }
        else {
            LOG(FATAL) << "Unknown codec type " << (int)codec_type_;
        }
    }
    if (result != AMF_OK) {
        LOG(ERR) << "AMFSurface::SetProperty(PICTURE_TYPE) failed with " << result;
    }
    result = encoder_->SubmitInput(surface);
    if (result != AMF_OK) {
        if (result == AMF_INVALID_RESOLUTION && last_submit_error_ == AMF_INVALID_RESOLUTION) {
            return nullptr;
        }
        else {
            LOG(ERR) << "AMFComponent::SubmitInput failed with " << result;
            last_submit_error_ = result;
            return nullptr;
        }
    }
    last_submit_error_ = AMF_OK;
    amf::AMFDataPtr outdata = nullptr;
    result = encoder_->QueryOutput(&outdata);
    if (result == AMF_EOF) {
        return nullptr;
    }
    if (outdata == nullptr) {
        LOG(ERR) << "AMFComponent::QueryOutput failed with " << result;
        return nullptr;
    }
    amf::AMFBufferPtr buffer{outdata};
    auto out_frame = std::make_shared<ltproto::client2worker::VideoFrame>();
    out_frame->set_is_keyframe(isKeyFrame(outdata));
    out_frame->set_frame(buffer->GetNative(), static_cast<uint32_t>(buffer->GetSize()));
    return out_frame;
}

bool AmdEncoderImpl::loadAmdApi() {
    std::string lib_name = AMF_DLL_NAMEA;
    amdapi_ = ltlib::DynamicLibrary::load(lib_name);
    if (amdapi_ == nullptr) {
        LOG(ERR) << "Load " << lib_name << " failed";
        return false;
    }
    auto AMFInit = reinterpret_cast<AMFInit_Fn>(amdapi_->getFunc(AMF_INIT_FUNCTION_NAME));
    if (AMFInit == nullptr) {
        LOG(ERR) << "Load 'AMFInit' from '" << lib_name << "' failed";
        return false;
    }
    AMF_RESULT result = AMFInit(AMF_FULL_VERSION, &factory_);
    if (result != AMF_OK) {
        LOG(ERR) << "AMFInit failed with " << result;
        return false;
    }
    return true;
}

bool AmdEncoderImpl::setAvcEncodeParams(const AmfParamsHelper& params) {
    AMF_RESULT result =
        encoder_->SetProperty(AMF_VIDEO_ENCODER_USAGE, AMF_VIDEO_ENCODER_USAGE_ULTRA_LOW_LATENCY);
    if (result != AMF_OK) {
        LOG(ERR) << "Set AMF_VIDEO_ENCODER_USAGE failed with " << result;
        return false;
    }
    result = encoder_->SetProperty(AMF_VIDEO_ENCODER_IDR_PERIOD, params.gop());
    if (result != AMF_OK) {
        LOG(ERR) << "Set AMF_VIDEO_ENCODER_IDR_PERIOD failed with " << result;
        return false;
    }
    result = encoder_->SetProperty(AMF_VIDEO_ENCODER_TARGET_BITRATE, params.bitrate());
    if (result != AMF_OK) {
        LOG(ERR) << "Set AMF_VIDEO_ENCODER_TARGET_BITRATE failed with " << result;
        return false;
    }
    result = encoder_->SetProperty(AMF_VIDEO_ENCODER_PEAK_BITRATE, params.max_bitrate());
    if (result != AMF_OK) {
        LOG(ERR) << "Set AMF_VIDEO_ENCODER_TARGET_BITRATE failed with " << result;
    }
    result = encoder_->SetProperty(AMF_VIDEO_ENCODER_MIN_QP, params.qmin());
    if (result != AMF_OK) {
        LOG(ERR) << "Set AMF_VIDEO_ENCODER_MIN_QP failed with " << result;
        return false;
    }
    result = encoder_->SetProperty(AMF_VIDEO_ENCODER_MAX_QP, params.qmax());
    if (result != AMF_OK) {
        LOG(ERR) << "Set AMF_VIDEO_ENCODER_MAX_QP failed with " << result;
        return false;
    }

    result = encoder_->SetProperty(AMF_VIDEO_ENCODER_QUALITY_PRESET, params.presetAvc());
    if (result != AMF_OK) {
        LOG(ERR) << "Set AMF_VIDEO_ENCODER_QUALITY_PRESET failed with " << result;
        return false;
    }
    result = encoder_->SetProperty(AMF_VIDEO_ENCODER_B_PIC_PATTERN, 0);
    if (result != AMF_OK) {
        LOG(ERR) << "Set AMF_VIDEO_ENCODER_B_PIC_PATTERN failed with " << result;
        return false;
    }
    result = encoder_->SetProperty(AMF_VIDEO_ENCODER_FRAMESIZE,
                                   ::AMFConstructSize(params.width(), params.height()));
    if (result != AMF_OK) {
        LOG(ERR) << "Set AMF_VIDEO_ENCODER_FRAMESIZE failed with " << result;
        return false;
    }
    result =
        encoder_->SetProperty(AMF_VIDEO_ENCODER_FRAMERATE, ::AMFConstructRate(params.fps(), 1));
    if (result != AMF_OK) {
        LOG(ERR) << "Set AMF_VIDEO_ENCODER_FRAMERATE failed with " << result;
        return false;
    }
    result = encoder_->SetProperty(AMF_VIDEO_ENCODER_ENFORCE_HRD, true); // ??
    if (result != AMF_OK) {
        LOG(ERR) << "Set AMF_VIDEO_ENCODER_ENFORCE_HRD failed with " << result;
        return false;
    }
    result = encoder_->SetProperty(AMF_VIDEO_ENCODER_RATE_CONTROL_METHOD, params.rc());
    if (result != AMF_OK) {
        LOG(ERR) << "Set AMF_VIDEO_ENCODER_RATE_CONTROL_METHOD failed with " << result;
        return false;
    }
    result = encoder_->SetProperty(AMF_VIDEO_ENCODER_LOWLATENCY_MODE, true);
    if (result != AMF_OK) {
        LOG(ERR) << "Set AMF_VIDEO_ENCODER_LOWLATENCY_MODE failed with " << result;
        return false;
    }
    return true;
}

bool AmdEncoderImpl::setHevcEncodeParams(const AmfParamsHelper& params) {
    AMF_RESULT result = encoder_->SetProperty(AMF_VIDEO_ENCODER_HEVC_USAGE,
                                              AMF_VIDEO_ENCODER_HEVC_USAGE_ULTRA_LOW_LATENCY);
    if (result != AMF_OK) {
        LOG(ERR) << "Set AMF_VIDEO_ENCODER_HEVC_USAGE failed with " << result;
        return false;
    }
    result = encoder_->SetProperty(AMF_VIDEO_ENCODER_HEVC_GOP_SIZE, params.gop());
    if (result != AMF_OK) {
        LOG(ERR) << "Set AMF_VIDEO_ENCODER_HEVC_GOP_SIZE failed with " << result;
        return false;
    }
    result = encoder_->SetProperty(AMF_VIDEO_ENCODER_HEVC_TARGET_BITRATE, params.bitrate());
    if (result != AMF_OK) {
        LOG(ERR) << "Set AMF_VIDEO_ENCODER_HEVC_TARGET_BITRATE failed with " << result;
        return false;
    }
    result = encoder_->SetProperty(AMF_VIDEO_ENCODER_HEVC_PEAK_BITRATE, params.max_bitrate());
    if (result != AMF_OK) {
        LOG(ERR) << "Set AMF_VIDEO_ENCODER_TARGET_BITRATE failed with " << result;
    }
    result = encoder_->SetProperty(AMF_VIDEO_ENCODER_HEVC_MIN_QP_P, params.qmin());
    if (result != AMF_OK) {
        LOG(ERR) << "Set AMF_VIDEO_ENCODER_HEVC_MIN_QP_P failed with " << result;
        return false;
    }
    result = encoder_->SetProperty(AMF_VIDEO_ENCODER_HEVC_MAX_QP_P, params.qmax());
    if (result != AMF_OK) {
        LOG(ERR) << "Set AMF_VIDEO_ENCODER_HEVC_MAX_QP_P failed with " << result;
        return false;
    }
    result = encoder_->SetProperty(AMF_VIDEO_ENCODER_HEVC_QUALITY_PRESET, params.presetHevc());
    if (result != AMF_OK) {
        LOG(ERR) << "Set AMF_VIDEO_ENCODER_HEVC_QUALITY_PRESET failed with " << result;
        return false;
    }
    // result = encoder_->SetProperty(AMF_VIDEO_ENCODER_B_PIC_PATTERN, 0);
    // if (result != AMF_OK) {
    //     LOG(ERR) << "Set AMF_VIDEO_ENCODER_B_PIC_PATTERN failed with " << result;
    //     return false;
    // }
    result = encoder_->SetProperty(AMF_VIDEO_ENCODER_HEVC_FRAMESIZE,
                                   ::AMFConstructSize(params.width(), params.height()));
    if (result != AMF_OK) {
        LOG(ERR) << "Set AMF_VIDEO_ENCODER_HEVC_FRAMESIZE failed with " << result;
        return false;
    }
    result = encoder_->SetProperty(AMF_VIDEO_ENCODER_HEVC_FRAMERATE,
                                   ::AMFConstructRate(params.fps(), 1));
    if (result != AMF_OK) {
        LOG(ERR) << "Set AMF_VIDEO_ENCODER_HEVC_FRAMERATE failed with " << result;
        return false;
    }
    result = encoder_->SetProperty(AMF_VIDEO_ENCODER_HEVC_ENFORCE_HRD, true); // ??
    if (result != AMF_OK) {
        LOG(ERR) << "Set AMF_VIDEO_ENCODER_HEVC_ENFORCE_HRD failed with " << result;
        return false;
    }
    result = encoder_->SetProperty(AMF_VIDEO_ENCODER_HEVC_RATE_CONTROL_METHOD, params.rc());
    if (result != AMF_OK) {
        LOG(ERR) << "Set AMF_VIDEO_ENCODER_HEVC_RATE_CONTROL_METHOD failed with " << result;
        return false;
    }
    result = encoder_->SetProperty(AMF_VIDEO_ENCODER_HEVC_LOWLATENCY_MODE, true);
    if (result != AMF_OK) {
        LOG(ERR) << "Set AMF_VIDEO_ENCODER_HEVC_LOWLATENCY_MODE failed with " << result;
        return false;
    }
    return true;
}

bool AmdEncoderImpl::isKeyFrame(amf::AMFDataPtr data) {
    if (codec_type_ == lt::VideoCodecType::H264) {
        amf_int64 type;
        AMF_RESULT result = data->GetProperty(AMF_VIDEO_ENCODER_OUTPUT_DATA_TYPE, &type);
        if (result == AMF_OK) {
            return type == AMF_VIDEO_ENCODER_OUTPUT_DATA_TYPE_IDR ||
                   type == AMF_VIDEO_ENCODER_OUTPUT_DATA_TYPE_I;
        }
    }
    else {
        amf_int64 type;
        AMF_RESULT result = data->GetProperty(AMF_VIDEO_ENCODER_HEVC_OUTPUT_DATA_TYPE, &type);
        if (result == AMF_OK) {
            return type == AMF_VIDEO_ENCODER_HEVC_OUTPUT_DATA_TYPE_IDR ||
                   type == AMF_VIDEO_ENCODER_HEVC_OUTPUT_DATA_TYPE_I;
        }
    }
    return false;
}

std::unique_ptr<AmdEncoder> AmdEncoder::create(const EncodeParamsHelper& params) {
    auto encoder = std::make_unique<AmdEncoder>();
    auto impl = std::make_shared<AmdEncoderImpl>(params);
    if (!impl->init()) {
        return nullptr;
    }
    encoder->impl_ = impl;
    return encoder;
}

void AmdEncoder::reconfigure(const Encoder::ReconfigureParams& params) {
    impl_->reconfigure(params);
}

CaptureFormat AmdEncoder::captureFormat() const {
    return CaptureFormat::D3D11_BGRA;
}

VideoCodecType AmdEncoder::codecType() const {
    return impl_->codecType();
}

uint32_t AmdEncoder::width() const {
    return impl_->width();
}

uint32_t AmdEncoder::height() const {
    return impl_->height();
}

std::shared_ptr<ltproto::client2worker::VideoFrame> AmdEncoder::encodeFrame(void* input_frame) {
    return impl_->encodeOneFrame(input_frame, needKeyframe());
}

bool AmdEncoder::doneFrame1() const {
    return false;
}

bool AmdEncoder::doneFrame2() const {
    return true;
}

} // namespace video

} // namespace lt
