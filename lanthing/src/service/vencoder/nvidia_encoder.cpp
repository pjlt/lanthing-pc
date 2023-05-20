#include "nvidia_encoder.h"
#include <cassert>
#include <string>

#include <d3d11.h>
#include <wrl/client.h>

#include <g3log/g3log.hpp>

#include <nvcodec/NvEncoderD3D11.h>
#include <nvcodec/NvEncoderCLIOptions.h>

namespace lt
{

namespace svc
{

class NvEncParamsHelper
{
public:
    NvEncParamsHelper(ltrtc::VideoCodecType c);

    NvEncParamsHelper& fps(int fps);

    NvEncParamsHelper& bitrate(int bitrate_kbps, bool enable_vbv = false);

    std::string params() const;

private:
    std::map<std::string, std::string> params_;

    int fps_ = 0;
    int bitrate_kbps_ = 0;
    bool enable_vbv_ = false;
};

class NvD3d11EncoderImpl
{
public:
    struct Params
    {
        uint32_t width = 0;
        uint32_t height = 0;
        int fps = 0;
    };

public:
    NvD3d11EncoderImpl();
    bool init(const VideoEncoder::InitParams& params);
    void reconfigure(const VideoEncoder::ReconfigureParams& params);
    VideoEncoder::EncodedFrame encode_one_frame(void* input_frame, bool request_iframe);

private:
    std::unique_ptr<NvEncoderD3D11> impl_;
    ltrtc::VideoCodecType codec_;
    std::optional<NvEncParamsHelper> params_;
    Microsoft::WRL::ComPtr<ID3D11Device> d3d_device_;
};

NvEncParamsHelper::NvEncParamsHelper(ltrtc::VideoCodecType c)
{
    assert(c == ltrtc::VideoCodecType::H264 || c == ltrtc::VideoCodecType::H265);
    std::string codec = c == ltrtc::VideoCodecType::H264 ? "h264" : "hevc";
    params_["-codec"] = codec;
    params_["-gop"] = "-1";
    params_["-rc"] = "vbr";
    params_["-preset"] = "ll_hp";
    params_["-profile"] = "main";
    params_["-qmin"] = "10,10,10";
    params_["-qmax"] = "40,40,40";
    params_["-bitrate"] = std::to_string(10 * 1024 * 1024);
    params_["-maxbitrate"] = std::to_string(10 * 1024 * 1024 * 1.05f);
}

NvEncParamsHelper& NvEncParamsHelper::fps(int fps)
{
    fps_ = fps;
    params_["-fps"] = std::to_string(fps);
    bitrate(bitrate_kbps_, enable_vbv_);
    return *this;
}

NvEncParamsHelper& NvEncParamsHelper::bitrate(int bitrate_kbps, bool enable_vbv)
{
    enable_vbv_ = enable_vbv;
    bitrate_kbps_ = bitrate_kbps;
    int bitrate_bps = bitrate_kbps_ * 1024;

    if (enable_vbv) {
        float vbv = 0.f;
        if (bitrate_bps >= 12 * 1024 * 1024) {
            params_["-qmin"] = "14,14,24";
            params_["-qmax"] = "39,39,39";
            vbv = 2.7f;
        } else if (bitrate_bps >= 8 * 1024 * 1024) {
            params_["-qmin"] = "15,15,24";
            params_["-qmax"] = "40,40,41";
            vbv = 2.6f;
        } else if (bitrate_bps >= 6 * 1024 * 1024) {
            params_["-qmin"] = "17,17,25";
            params_["-qmax"] = "42,42,42";
            vbv = 2.4f;
        } else if (bitrate_bps >= 4 * 1024 * 1024) {
            params_["-qmin"] = "18,18,26";
            params_["-qmax"] = "43,43,42";
            vbv = 2.3f;
        } else if (bitrate_bps >= 3 * 1024 * 1024) {
            params_["-qmin"] = "19,19,27";
            params_["-qmax"] = "44,44,43";
            vbv = 2.1f;
        } else {
            params_["-qmin"] = "21,21,28";
            params_["-qmax"] = "47,47,46";
            vbv = 2.1f;
        }
        int bitrate_vbv = static_cast<int>(bitrate_bps * vbv + 0.5f);
        int vbv_buf = static_cast<int>(bitrate_vbv * 1.0f / fps_ + 0.5f);
        params_["-vbvbufsize"] = std::to_string(vbv_buf);
        params_["-vbvinit"] = std::to_string(vbv_buf);
    } else {
        params_.erase("-vbvbufsize");
        params_.erase("-vbvinit");
    }

    params_["-bitrate"] = std::to_string(bitrate_bps);
    params_["-maxbitrate"] = std::to_string(bitrate_bps * 1.05f);

    return *this;
}

std::string NvEncParamsHelper::params() const
{
    std::stringstream oss;
    for (const auto& param : params_) {
        if (param.first.empty() || param.second.empty()) {
            continue;
        }
        oss << param.first << " " << param.second << " ";
    }
    return oss.str();
}

NvD3d11EncoderImpl::NvD3d11EncoderImpl()
{
}

NvD3d11EncoderImpl::~NvD3d11EncoderImpl()
{
}

bool NvD3d11EncoderImpl::init(const VideoEncoder::InitParams& params)
{
    d3d_device_ = reinterpret_cast<ID3D11Device*>(params.context);
    codec_ = params.codec_type;
    params_ = NvEncParamsHelper { codec_ };
    impl_ = std::make_unique<NvEncoderD3D11>(d3d_device_.Get(), params.width, params.height,
        NV_ENC_BUFFER_FORMAT_ARGB, 0);

    NV_ENC_INITIALIZE_PARAMS init_params = { NV_ENC_INITIALIZE_PARAMS_VER };
    NV_ENC_CONFIG config = { NV_ENC_CONFIG_VER };
    init_params.encodeConfig = &config;
    auto cli_ops = NvEncoderInitParam(params_->params().c_str());
    impl_->CreateDefaultEncoderParams(&init_params, cli_ops.GetEncodeGUID(),
        cli_ops.GetPresetGUID());
    if (init_params.encodeGUID == NV_ENC_CODEC_H264_GUID) {
        init_params.encodeConfig->encodeCodecConfig.h264Config.maxNumRefFrames = 0;
        init_params.encodeConfig->encodeCodecConfig.h264Config.sliceMode = 3;
        init_params.encodeConfig->encodeCodecConfig.h264Config.sliceModeData = 1;
    }
    if (init_params.encodeGUID == NV_ENC_CODEC_HEVC_GUID) {
        init_params.encodeConfig->encodeCodecConfig.hevcConfig.maxNumRefFramesInDPB = 0;
        init_params.encodeConfig->encodeCodecConfig.hevcConfig.sliceMode = 3;
        init_params.encodeConfig->encodeCodecConfig.hevcConfig.sliceModeData = 1;
    }
    cli_ops.SetInitParams(&init_params, NV_ENC_BUFFER_FORMAT_ARGB);

    try {
        impl_->CreateEncoder(&init_params);
    } catch (const NVENCException& exception) {
        LOGF(INFO, "nvenc exception: %s", exception.what());
        return false;
    }

    return true;
}

VideoEncoder::EncodedFrame NvD3d11EncoderImpl::encode_one_frame(void* input_frame, bool request_iframe)
{
    NV_ENC_PIC_PARAMS pic_params = { NV_ENC_PIC_PARAMS_VER };
    if (request_iframe) {
        pic_params.encodePicFlags = NV_ENC_PIC_FLAG_FORCEIDR | NV_ENC_PIC_FLAG_OUTPUT_SPSPPS;
    }
    VideoEncoder::EncodedFrame out_frame {};
    std::vector<std::vector<uint8_t>> imgs;
    auto type = impl_->EncodeFrame(input_frame, imgs, &pic_params);
    if (!imgs.empty()) {
        out_frame.size = imgs.back().size();
        out_frame.data = std::make_shared<uint8_t>(new uint8_t[out_frame.size]);
        memcpy(out_frame.data.get(), imgs.back().data(), out_frame.size);
        out_frame.is_keyframe = type == NV_ENC_PIC_TYPE_I || type == NV_ENC_PIC_TYPE_IDR;
    }
    return out_frame;
}

void NvD3d11EncoderImpl::reconfigure(const VideoEncoder::ReconfigureParams& p)
{
    bool changed = false;
    if (p.bitrate_bps.has_value()) {
        params_->bitrate(p.bitrate_bps.value() / 1024);
        changed = true;
    }
    if (p.fps.has_value()) {
        params_->fps(p.fps.value());
    }
    auto cli_ops = NvEncoderInitParam(params_->params().c_str());
    NV_ENC_RECONFIGURE_PARAMS params = { NV_ENC_RECONFIGURE_PARAMS_VER };
    NV_ENC_CONFIG config = { NV_ENC_CONFIG_VER };
    params.reInitEncodeParams.encodeConfig = &config;
    impl_->GetInitializeParams(&params.reInitEncodeParams);
    cli_ops.SetInitParams(&params.reInitEncodeParams, NV_ENC_BUFFER_FORMAT_ARGB);
    if (impl_->Reconfigure(&params)) {
        LOGF(WARNING, "reconfig NvEnc failed, params: %s",
            cli_ops.MainParamToString(&params.reInitEncodeParams).c_str());
    }
}

NvD3d11Encoder::NvD3d11Encoder()
{
}

NvD3d11Encoder::~NvD3d11Encoder()
{
}

bool NvD3d11Encoder::init(const InitParams& params)
{
    return impl_->init(params);
}

void NvD3d11Encoder::reconfigure(const ReconfigureParams& params)
{
    impl_->reconfigure(params);
}

NvD3d11Encoder::EncodedFrame NvD3d11Encoder::encode_one_frame(void* input_frame, bool request_iframe)
{
    return impl_->encode_one_frame(input_frame, request_iframe);
}

} // namespace svc

} // namespace lt
