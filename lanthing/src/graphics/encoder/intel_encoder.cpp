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

#define ONEVPL_EXPERIMENTAL 1
#include "intel_encoder.h"
#include "intel_allocator.h"

#include <d3d11.h>
#include <wrl/client.h>

#include <vpl/mfx.h>

#include <g3log/g3log.hpp>

#define MSDK_ALIGN16(value) (((value + 15) >> 4) << 4)
#define MSDK_ALIGN32(X) (((mfxU32)((X) + 31)) & (~(mfxU32)31))

using namespace Microsoft::WRL;

namespace {

mfxStatus ConvertFrameRate(mfxF64 dFrameRate, mfxU32* pnFrameRateExtN, mfxU32* pnFrameRateExtD) {
    mfxU32 fr;

    fr = (mfxU32)(dFrameRate + .5);

    if (fabs(fr - dFrameRate) < 0.0001) {
        *pnFrameRateExtN = fr;
        *pnFrameRateExtD = 1;
        return MFX_ERR_NONE;
    }

    fr = (mfxU32)(dFrameRate * 1.001 + .5);

    if (fabs(fr * 1000 - dFrameRate * 1001) < 10) {
        *pnFrameRateExtN = fr * 1000;
        *pnFrameRateExtD = 1001;
        return MFX_ERR_NONE;
    }

    *pnFrameRateExtN = (mfxU32)(dFrameRate * 10000 + .5);
    *pnFrameRateExtD = 10000;

    return MFX_ERR_NONE;
}

mfxU16 FourCCToChroma(mfxU32 fourCC) {
    switch (fourCC) {
    case MFX_FOURCC_NV12:
    case MFX_FOURCC_P010:
        return MFX_CHROMAFORMAT_YUV420;
    case MFX_FOURCC_NV16:
    case MFX_FOURCC_P210:
#if (MFX_VERSION >= 1027)
    case MFX_FOURCC_Y210:
#endif
    case MFX_FOURCC_YUY2:
    case MFX_FOURCC_UYVY:
        return MFX_CHROMAFORMAT_YUV422;
#if (MFX_VERSION >= 1027)
    case MFX_FOURCC_Y410:
    case MFX_FOURCC_A2RGB10:
#endif
    case MFX_FOURCC_AYUV:
    case MFX_FOURCC_RGB4:
        return MFX_CHROMAFORMAT_YUV444;
    }

    return MFX_CHROMAFORMAT_YUV420;
}

void printMfxVideoParamEncode(const mfxVideoParam& p) {
    LOGF(
        INFO,
        "AsyncDepth:%u, IOPattern:%u, NumExtParam:%u, LowPower:%u, BRCParamMultiplier:%u, "
        "CodecId:%u, CodecProfile:%u, CodecLevel:%u, NumThread:%u, TargetUsage:%u, GopPicSize:%u, "
        "GopRefDist:%u, GopOptFlag:%u, IdrInterval:%u, RateControlMethod:%u, InitialDelayInKB:%u, "
        "BufferSizeInKB:%u, TargetKbps:%u, MaxKbps:%u, NumSlice:%u, NumRefFrame:%u, "
        "EncodedOrder:%u, FrameInfo{ChannelId:%u, BitDepthLuma:%u, BitDepthChroma:%u, Shift:%u, "
        "FourCC:%u, Width:%u, Height:%u, CropX:%u, CropY:%u, CropW:%u, CropH:%u, FrameRateExtN:%u, "
        "FrameRateExtD:%u, AspectRatioW:%u, AspectRatioH:%u, PicStruct:%u, ChromaFormat:%u}",
        p.AsyncDepth, p.IOPattern, p.NumExtParam, p.mfx.LowPower, p.mfx.BRCParamMultiplier,
        p.mfx.CodecId, p.mfx.CodecProfile, p.mfx.CodecLevel, p.mfx.NumThread, p.mfx.TargetUsage,
        p.mfx.GopPicSize, p.mfx.GopRefDist, p.mfx.GopOptFlag, p.mfx.IdrInterval,
        p.mfx.RateControlMethod, p.mfx.InitialDelayInKB, p.mfx.BufferSizeInKB, p.mfx.TargetKbps,
        p.mfx.MaxKbps, p.mfx.NumSlice, p.mfx.NumRefFrame, p.mfx.EncodedOrder,
        p.mfx.FrameInfo.ChannelId, p.mfx.FrameInfo.BitDepthLuma, p.mfx.FrameInfo.BitDepthChroma,
        p.mfx.FrameInfo.Shift, p.mfx.FrameInfo.FourCC, p.mfx.FrameInfo.Width,
        p.mfx.FrameInfo.Height, p.mfx.FrameInfo.CropX, p.mfx.FrameInfo.CropY, p.mfx.FrameInfo.CropW,
        p.mfx.FrameInfo.CropH, p.mfx.FrameInfo.FrameRateExtN, p.mfx.FrameInfo.FrameRateExtD,
        p.mfx.FrameInfo.AspectRatioW, p.mfx.FrameInfo.AspectRatioH, p.mfx.FrameInfo.PicStruct,
        p.mfx.FrameInfo.ChromaFormat);
}

void printMfxVideoParamVPP(const mfxVideoParam& p) {
    LOGF(
        INFO,
        "AsyncDepth:%u, IOPattern:%u, NumExtParam:%u, VppIn{ChannelId:%u, BitDepthLuma:%u, "
        "BitDepthChroma:%u, Shift:%u, "
        "FourCC:%u, Width:%u, Height:%u, CropX:%u, CropY:%u, CropW:%u, CropH:%u, FrameRateExtN:%u, "
        "FrameRateExtD:%u, AspectRatioW:%u, AspectRatioH:%u, PicStruct:%u, ChromaFormat:%u}, "
        "VppOut{ChannelId:%u, BitDepthLuma:%u, BitDepthChroma:%u, Shift:%u, "
        "FourCC:%u, Width:%u, Height:%u, CropX:%u, CropY:%u, CropW:%u, CropH:%u, FrameRateExtN:%u, "
        "FrameRateExtD:%u, AspectRatioW:%u, AspectRatioH:%u, PicStruct:%u, ChromaFormat:%u}",
        p.AsyncDepth, p.IOPattern, p.NumExtParam, p.vpp.In.ChannelId, p.vpp.In.BitDepthLuma,
        p.vpp.In.BitDepthChroma, p.vpp.In.Shift, p.vpp.In.FourCC, p.vpp.In.Width, p.vpp.In.Height,
        p.vpp.In.CropX, p.vpp.In.CropY, p.vpp.In.CropW, p.vpp.In.CropH, p.vpp.In.FrameRateExtN,
        p.vpp.In.FrameRateExtD, p.vpp.In.AspectRatioW, p.vpp.In.AspectRatioH, p.vpp.In.PicStruct,
        p.vpp.In.ChromaFormat, p.vpp.Out.ChannelId, p.vpp.Out.BitDepthLuma,
        p.vpp.Out.BitDepthChroma, p.vpp.Out.Shift, p.vpp.Out.FourCC, p.vpp.Out.Width,
        p.vpp.Out.Height, p.vpp.Out.CropX, p.vpp.Out.CropY, p.vpp.Out.CropW, p.vpp.Out.CropH,
        p.vpp.Out.FrameRateExtN, p.vpp.Out.FrameRateExtD, p.vpp.Out.AspectRatioW,
        p.vpp.Out.AspectRatioH, p.vpp.Out.PicStruct, p.vpp.Out.ChromaFormat);
}

class VplParamsHelper {
public:
    VplParamsHelper(const lt::VideoEncodeParamsHelper& params)
        : params_{params} {}

    mfxU32 codec() const;
    int fps() const { return params_.fps(); }
    int64_t gop() const { return params_.gop() < 0 ? 0 : params_.gop(); }
    int64_t bitrate() const { return params_.bitrate(); }
    mfxU16 bitrate_kbps() const { return static_cast<mfxU16>(params_.bitrate_kbps()); };
    mfxU16 maxbitrate_kbps() const { return static_cast<mfxU16>(params_.maxbitrate_kbps()); }
    int64_t qmin() const { return params_.qmin()[0]; }
    int64_t qmax() const { return params_.qmax()[0]; }
    mfxU16 rc() const;
    mfxU16 preset() const;
    mfxU16 profile() const;

private:
    const lt::VideoEncodeParamsHelper params_;
};

mfxU32 VplParamsHelper::codec() const {
    return params_.codec() == lt::VideoCodecType::H264 ? MFX_CODEC_AVC : MFX_CODEC_HEVC;
}

mfxU16 VplParamsHelper::rc() const {
    switch (params_.rc()) {
    case lt::VideoEncodeParamsHelper::RcMode::CBR:
        return MFX_RATECONTROL_CBR;
    case lt::VideoEncodeParamsHelper::RcMode::VBR:
        return MFX_RATECONTROL_VBR;
    default:
        assert(false);
        return MFX_RATECONTROL_VBR;
    }
}

mfxU16 VplParamsHelper::preset() const {
    switch (params_.preset()) {
    case lt::VideoEncodeParamsHelper::Preset::Balanced:
        return MFX_TARGETUSAGE_BALANCED;
    case lt::VideoEncodeParamsHelper::Preset::Speed:
        return MFX_TARGETUSAGE_BEST_SPEED;
    case lt::VideoEncodeParamsHelper::Preset::Quality:
        return MFX_TARGETUSAGE_BEST_QUALITY;
    default:
        assert(false);
        return MFX_TARGETUSAGE_UNKNOWN;
    }
}

mfxU16 VplParamsHelper::profile() const {
    switch (params_.profile()) {
    case lt::VideoEncodeParamsHelper::Profile::AvcMain:
        return MFX_PROFILE_AVC_MAIN;
    case lt::VideoEncodeParamsHelper::Profile::HevcMain:
        return MFX_PROFILE_HEVC_MAIN;
    default:
        assert(false);
        return MFX_PROFILE_AVC_MAIN;
    }
}

struct VplSize {
    uint16_t factor = 0;
    uint32_t init_delay = 0;
    uint32_t buffer_size = 0;
    uint32_t target = 0;
    uint32_t max = 0;
};

VplSize calcSize(VplSize oldsize) {
    if (oldsize.factor == 0) {
        oldsize.factor = 1;
    }
    VplSize newsize;
    newsize.buffer_size = oldsize.buffer_size * oldsize.factor;
    newsize.init_delay = oldsize.init_delay * oldsize.factor;
    newsize.max = oldsize.max * oldsize.factor;
    newsize.target = oldsize.target * oldsize.factor;
    uint32_t themax =
        std::max({newsize.buffer_size, newsize.init_delay, newsize.max, newsize.target});
    newsize.factor = static_cast<uint16_t>((themax + 65536) / 65536);
    newsize.buffer_size /= newsize.factor;
    newsize.init_delay /= newsize.factor;
    newsize.max /= newsize.factor;
    newsize.target /= newsize.factor;
    return newsize;
}

} // namespace

namespace lt {

class IntelEncoderImpl {
public:
    IntelEncoderImpl(ID3D11Device* d3d11_dev, ID3D11DeviceContext* d3d11_ctx, int64_t luid);
    ~IntelEncoderImpl();
    bool init(const VideoEncodeParamsHelper& params);
    void reconfigure(const VideoEncoder::ReconfigureParams& params);
    std::shared_ptr<ltproto::peer2peer::VideoFrame> encodeOneFrame(void* input_frame,
                                                                   bool request_iframe);

private:
    bool createMfxSession();
    bool setConfigFilter();
    bool findImplIndex();
    bool initEncoder(const VplParamsHelper& params_helper);
    bool initVpp(const VplParamsHelper& params_helper);
    ComPtr<ID3D11Texture2D> allocEncodeTexture();
    mfxVideoParam genEncodeParams(const VplParamsHelper& params_helper);
    mfxVideoParam genVppParams(const VplParamsHelper& params_helper);

private:
    ComPtr<ID3D11Device> d3d11_dev_;
    ComPtr<ID3D11DeviceContext> d3d11_ctx_;
    ComPtr<ID3D11Texture2D> encode_texture_;
    const int64_t luid_;
    int32_t impl_index_ = -1;
    uint32_t width_;
    uint32_t height_;
    lt::VideoCodecType codec_type_;
    mfxLoader mfxloader_ = nullptr;
    mfxSession mfxsession_ = nullptr;
    mfxVideoParam encode_param_{};
    mfxVideoParam vpp_param_{};
    std::unique_ptr<MfxEncoderFrameAllocator> allocator_;
    mfxExtVPPVideoSignalInfo vpp_signal_{};
    std::vector<mfxExtBuffer*> vpp_ext_buffers_;
    mfxExtCodingOption enc_coding_opt_{};
    std::vector<mfxExtBuffer*> enc_ext_buffers_;
    std::vector<uint8_t> bitstream_;
};

IntelEncoderImpl::IntelEncoderImpl(ID3D11Device* d3d11_dev, ID3D11DeviceContext* d3d11_ctx,
                                   int64_t luid)
    : d3d11_dev_{d3d11_dev}
    , d3d11_ctx_{d3d11_ctx}
    , luid_{luid} {}

IntelEncoderImpl::~IntelEncoderImpl() {
    if (mfxloader_ != nullptr) {
        MFXUnload(mfxloader_);
    }
}

bool IntelEncoderImpl::init(const VideoEncodeParamsHelper& params) {
    width_ = params.width();
    height_ = params.height();
    codec_type_ = params.codec();
    Microsoft::WRL::ComPtr<ID3D10Multithread> tmp10;
    auto hr = d3d11_ctx_.As(&tmp10);
    if (FAILED(hr)) {
        LOG(WARNING) << "Cast to ID3D10Multithread failed with " << GetLastError();
        return false;
    }
    tmp10->SetMultithreadProtected(true);
    // 我无法理解设计出这个“allocator”的人是怎么想的
    allocator_ = std::make_unique<MfxEncoderFrameAllocator>(d3d11_dev_, d3d11_ctx_);
    mfxloader_ = MFXLoad();
    if (mfxloader_ == nullptr) {
        LOG(WARNING) << "MFXLoad failed";
        return false;
    }
    if (!createMfxSession()) {
        return false;
    }
    mfxStatus status =
        MFXVideoCORE_SetHandle(mfxsession_, MFX_HANDLE_D3D11_DEVICE, d3d11_dev_.Get());
    if (status != MFX_ERR_NONE) {
        LOGF(WARNING, "MFXVideoCORE_SetHandle(MFX_HANDLE_D3D11_DEVICE, %p) failed with %d",
             d3d11_dev_.Get(), status);
        return false;
    }
    status = MFXVideoCORE_SetFrameAllocator(mfxsession_, allocator_.get());
    if (status != MFX_ERR_NONE) {
        LOG(WARNING) << "MFXVideoCORE_SetFrameAllocator failed with " << status;
        return false;
    }

    VplParamsHelper params_helper{params};
    if (!initEncoder(params_helper)) {
        return false;
    }
    if (!initVpp(params_helper)) {
        return false;
    }
    return true;
}

void IntelEncoderImpl::reconfigure(const VideoEncoder::ReconfigureParams& params) {
    if (!params.bitrate_bps.has_value() && !params.fps.has_value()) {
        return;
    }
    if (params.bitrate_bps.has_value()) {
        uint32_t target_kbps = params.bitrate_bps.value() / 1024;
        uint32_t old8 = static_cast<uint32_t>(static_cast<uint32_t>(encode_param_.mfx.TargetKbps) *
                                              encode_param_.mfx.BRCParamMultiplier * 0.8);
        VplSize vsize;
        vsize.target = std::max(old8, target_kbps);
        vsize.max = static_cast<uint32_t>(vsize.target * 1.05);
        vsize.init_delay = static_cast<uint32_t>(encode_param_.mfx.InitialDelayInKB) *
                           encode_param_.mfx.BRCParamMultiplier;
        vsize.buffer_size = static_cast<uint32_t>(encode_param_.mfx.BufferSizeInKB) *
                            encode_param_.mfx.BRCParamMultiplier;
        vsize = calcSize(vsize);
        encode_param_.mfx.TargetKbps = static_cast<mfxU16>(vsize.target);
        encode_param_.mfx.MaxKbps = static_cast<mfxU16>(vsize.max);
        encode_param_.mfx.InitialDelayInKB = static_cast<mfxU16>(vsize.init_delay);
        encode_param_.mfx.BufferSizeInKB = static_cast<mfxU16>(vsize.buffer_size);
        encode_param_.mfx.BRCParamMultiplier = vsize.factor;
        LOG(DEBUG) << "factor:" << vsize.factor << ", TargetKbps " << encode_param_.mfx.TargetKbps
                   << ", MaxKbps:" << encode_param_.mfx.MaxKbps
                   << ", InitDelayInKB:" << encode_param_.mfx.InitialDelayInKB
                   << ", BufferSizeInKB:" << encode_param_.mfx.BufferSizeInKB;
    }
    if (params.fps.has_value()) {
        ConvertFrameRate(params.fps.value(), &encode_param_.mfx.FrameInfo.FrameRateExtN,
                         &encode_param_.mfx.FrameInfo.FrameRateExtD);
    }
    // LOG(INFO) << "before query";
    // printMfxVideoParamEncode(encode_param_);
    mfxStatus status = MFXVideoENCODE_Query(mfxsession_, &encode_param_, &encode_param_);
    if (status > MFX_ERR_NONE) {
        LOG(WARNING) << "MFXVideoENCODE_Query invalid parameters";
        // 是合法的留下？？？
        // LOG(INFO) << "after query";
        // printMfxVideoParamEncode(encode_param_);
        status = MFXVideoENCODE_Query(mfxsession_, &encode_param_, &encode_param_);
        // LOG(INFO) << "after query2";
        // printMfxVideoParamEncode(encode_param_);
    }
    if (status < MFX_ERR_NONE) {
        LOG(WARNING) << "MFXVideoENCODE_Query failed";
        return;
    }
    status = MFXVideoENCODE_Reset(mfxsession_, &encode_param_);
    // LOG(INFO) << "after reset";
    // printMfxVideoParamEncode(encode_param_);
    if (status != MFX_ERR_NONE) {
        LOG(WARNING) << "MFXVideoENCODE_Reset failed with " << status;
    }
}

std::shared_ptr<ltproto::peer2peer::VideoFrame>
IntelEncoderImpl::encodeOneFrame(void* input_frame, bool request_iframe) {
    mfxSyncPoint sync_point{};
    const uint32_t k1024 = 1024;
    const uint32_t buffer_size =
        k1024 * encode_param_.mfx.BufferSizeInKB * encode_param_.mfx.BRCParamMultiplier;
    if (bitstream_.size() < buffer_size) {
        bitstream_.resize(buffer_size);
    }
    mfxBitstream bs{};
    bs.Data = bitstream_.data();
    bs.MaxLength = static_cast<mfxU32>(bitstream_.size());
    mfxEncodeCtrl ctrl{};
    mfxEncodeCtrl* pctrl = nullptr;
    if (request_iframe) {
        // 这个FrameType应该填单个类型还是或起来？
        ctrl.FrameType = MFX_FRAMETYPE_I | MFX_FRAMETYPE_IDR;
        pctrl = &ctrl;
    }
    mfxFrameSurface1 vpp_surface{};
    vpp_surface.Data.MemId = input_frame;
    vpp_surface.Info = vpp_param_.mfx.FrameInfo;
    mfxFrameSurface1 encode_surface{};
    encode_surface.Data.MemId = encode_texture_.Get();
    encode_surface.Info = encode_param_.mfx.FrameInfo;
    mfxStatus status = MFX_ERR_NONE;
    while (true) {
        status = MFXVideoVPP_RunFrameVPPAsync(mfxsession_, &vpp_surface, &encode_surface, nullptr,
                                              &sync_point);
        if (status == MFX_WRN_DEVICE_BUSY) {
            std::this_thread::sleep_for(std::chrono::milliseconds{1});
            continue;
        }
        else if (status >= MFX_ERR_NONE) {
            break;
        }
        else {
            LOG(WARNING) << "MFXVideoVPP_RunFrameVPPAsync failed with " << status;
            return nullptr;
        }
    }
    while (true) {
        status =
            MFXVideoENCODE_EncodeFrameAsync(mfxsession_, pctrl, &encode_surface, &bs, &sync_point);
        if (status == MFX_WRN_DEVICE_BUSY) {
            std::this_thread::sleep_for(std::chrono::milliseconds{1});
            continue;
        }
        else if (status >= MFX_ERR_NONE) {
            break;
        }
        else if (status == MFX_ERR_NOT_ENOUGH_BUFFER) {
            LOG(WARNING) << "MFXVideoENCODE_EncodeFrameAsync failed with MFX_ERR_NOT_ENOUGH_BUFFER";
            assert(false);
            break;
        }
        else {
            LOG(INFO) << "MFXVideoENCODE_EncodeFrameAsync failed with " << status;
            return nullptr;
        }
    }
    status = MFX_WRN_IN_EXECUTION;
    while (status > 0) {
        status = MFXVideoCORE_SyncOperation(mfxsession_, sync_point, 2000);
        if (status == MFX_ERR_NONE) {
            break;
        }
        else if (status < MFX_ERR_NONE) {
            LOG(INFO) << "MFXVideoCORE_SyncOperation failed with " << status;
            return nullptr;
        }
        else {
            std::this_thread::sleep_for(std::chrono::milliseconds{1});
        }
    }
    bool is_keyframe = (bs.FrameType & MFX_FRAMETYPE_I) || (bs.FrameType & MFX_FRAMETYPE_IDR);
    auto out_frame = std::make_shared<ltproto::peer2peer::VideoFrame>();
    out_frame->set_frame(bitstream_.data(), bs.DataLength);
    out_frame->set_is_keyframe(is_keyframe);
    return out_frame;
}

bool IntelEncoderImpl::createMfxSession() {
    if (!setConfigFilter()) {
        return false;
    }
    if (!findImplIndex()) {
        return false;
    }
    mfxStatus status = MFXCreateSession(mfxloader_, impl_index_, &mfxsession_);
    if (status != MFX_ERR_NONE) {
        LOG(WARNING) << "MFXCreateSession failed with " << status;
        return false;
    }
    LOG(INFO) << "Created mfx session(" << impl_index_ << ")";
    return true;
}

bool IntelEncoderImpl::setConfigFilter() {
    mfxConfig cfg_hw = MFXCreateConfig(mfxloader_);
    mfxVariant val_hw{};
    val_hw.Type = MFX_VARIANT_TYPE_U32;
    val_hw.Data.U32 = MFX_IMPL_TYPE_HARDWARE;
    mfxStatus status =
        MFXSetConfigFilterProperty(cfg_hw, (const mfxU8*)"mfxImplDescription.Impl", val_hw);
    if (status != MFX_ERR_NONE) {
        LOG(WARNING) << "MFXSetConfigFilterProperty(mfxImplDescription.Impl=MFX_IMPL_TYPE_HARDWARE)"
                        " failed with "
                     << status;
        return false;
    }
    mfxConfig cfg_d3d11 = MFXCreateConfig(mfxloader_);
    mfxVariant val_d3d11{};
    val_d3d11.Type = MFX_VARIANT_TYPE_U32;
    val_d3d11.Data.U32 = MFX_ACCEL_MODE_VIA_D3D11;
    status = MFXSetConfigFilterProperty(
        cfg_d3d11, (const mfxU8*)"mfxImplDescription.AccelerationMode", val_d3d11);
    if (status != MFX_ERR_NONE) {
        LOG(WARNING) << "MFXSetConfigFilterProperty(mfxImplDescription.AccelerationMode=MFX_ACCEL_"
                        "MODE_VIA_D3D11)"
                        " failed with "
                     << status;
        return false;
    }
    return true;
}

bool IntelEncoderImpl::findImplIndex() {
    mfxStatus status = MFX_ERR_NONE;
    mfxExtendedDeviceId* ext_devid = nullptr;
    for (int index = 0; status == MFX_ERR_NONE; index++) {
        if (ext_devid != nullptr) {
            MFXDispReleaseImplDescription(mfxloader_, ext_devid);
            ext_devid = nullptr;
        }
        status = MFXEnumImplementations(mfxloader_, index, MFX_IMPLCAPS_DEVICE_ID_EXTENDED,
                                        (mfxHDL*)&ext_devid);
        if (status != MFX_ERR_NONE) {
            continue;
        }
        if (!ext_devid->LUIDValid) {
            continue;
        }
        const int64_t luid = *reinterpret_cast<int64_t*>(&ext_devid->DeviceLUID[0]);
        LOG(DEBUG) << "Set luid " << luid_ << ", get luid " << luid;
        if (luid == luid_) {
            impl_index_ = index;
            break;
        }
    }
    if (ext_devid != nullptr) {
        MFXDispReleaseImplDescription(mfxloader_, ext_devid);
        ext_devid = nullptr;
    }
    return impl_index_ >= 0;
}

bool IntelEncoderImpl::initEncoder(const VplParamsHelper& params_helper) {
    auto params = genEncodeParams(params_helper);
    // LOG(INFO) << "Generated VPL encode params";
    // printMfxVideoParamEncode(params);
    mfxStatus status = MFX_ERR_NONE;
    status = MFXVideoENCODE_Init(mfxsession_, &params);
    if (status < MFX_ERR_NONE) { // FIXME: 这里会有参数不兼容的警告
        LOG(WARNING) << "MFXVideoENCODE_Init failed with " << status;
        return false;
    }
    else if (status > MFX_ERR_NONE) {
        LOG(WARNING) << "MFXVideoENCODE_Init warn " << status;
    }
    mfxVideoParam param_out{};
    status = MFXVideoENCODE_GetVideoParam(mfxsession_, &param_out);
    if (status != MFX_ERR_NONE) {
        LOG(WARNING) << "MFXVideoENCODE_GetVideoParam failed with " << status;
        return false;
    }
    // LOG(INFO) << "GetVideoParam encode";
    // printMfxVideoParamEncode(param_out);
    encode_param_ = param_out;
    return true;
}

bool IntelEncoderImpl::initVpp(const VplParamsHelper& params_helper) {
    mfxVideoParam params = genVppParams(params_helper);
    LOG(INFO) << "Generated VPL vpp params";
    printMfxVideoParamVPP(params);
    encode_texture_ = allocEncodeTexture();
    mfxStatus status = MFXVideoVPP_Init(mfxsession_, &params);
    if (status != MFX_ERR_NONE) {
        LOG(WARNING) << "MFXVideoVPP_Init failed with " << status;
        return false;
    }
    mfxVideoParam param_out{};
    status = MFXVideoVPP_GetVideoParam(mfxsession_, &param_out);
    if (status != MFX_ERR_NONE) {
        LOG(WARNING) << "MFXVideoVPP_GetVideoParam failed with " << status;
        return false;
    }
    LOG(INFO) << "GetVideoParam vpp";
    printMfxVideoParamVPP(param_out);
    vpp_param_ = param_out;
    return true;
}

Microsoft::WRL::ComPtr<ID3D11Texture2D> IntelEncoderImpl::allocEncodeTexture() {
    Microsoft::WRL::ComPtr<ID3D11Texture2D> frame;
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = MSDK_ALIGN16(static_cast<mfxU16>(width_));
    desc.Height = MSDK_ALIGN16(static_cast<mfxU16>(height_));
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.Format = DXGI_FORMAT_NV12;
    desc.BindFlags = D3D11_BIND_RENDER_TARGET;
    desc.MiscFlags = 0;
    // desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
    HRESULT hr = d3d11_dev_->CreateTexture2D(&desc, nullptr, frame.GetAddressOf());
    if (FAILED(hr)) {
        // assert(false);
        LOG(WARNING) << "D3D11Device::CreateTexture2D failed with " << GetLastError();
        return nullptr;
    }
    return frame;
}

mfxVideoParam IntelEncoderImpl::genEncodeParams(const VplParamsHelper& params_helper) {
    VplSize vsize;
    vsize.buffer_size = 512;
    vsize.max = params_helper.maxbitrate_kbps();
    vsize.target = params_helper.bitrate_kbps();
    vsize = calcSize(vsize);
    mfxVideoParam params{};
    params.mfx.CodecId = params_helper.codec();
    params.mfx.LowPower = MFX_CODINGOPTION_OFF; // 效果差？
    params.mfx.TargetUsage = params_helper.preset();
    params.mfx.BRCParamMultiplier = vsize.factor;
    params.mfx.TargetKbps = static_cast<mfxU16>(vsize.target);
    params.mfx.RateControlMethod = params_helper.rc();
    params.mfx.GopRefDist = 1;
    params.mfx.GopPicSize = 0;
    params.mfx.NumRefFrame = 1;
    params.mfx.IdrInterval = 0; // TODO: 让avc和hevc行为一致
    params.mfx.CodecProfile = params_helper.profile();
    params.mfx.CodecLevel = 0; // 未填
    params.mfx.MaxKbps = static_cast<mfxU16>(vsize.max);
    params.mfx.InitialDelayInKB = static_cast<mfxU16>(vsize.init_delay);
    params.mfx.GopOptFlag = MFX_GOP_CLOSED;
    params.mfx.BufferSizeInKB = static_cast<mfxU16>(vsize.buffer_size);
    params.mfx.NumSlice = 0; // 未填值
    params.mfx.EncodedOrder = 0;
    params.IOPattern = MFX_IOPATTERN_IN_VIDEO_MEMORY;

    ConvertFrameRate(params_helper.fps(), &params.mfx.FrameInfo.FrameRateExtN,
                     &params.mfx.FrameInfo.FrameRateExtD);

    params.mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
    params.mfx.FrameInfo.ChromaFormat = FourCCToChroma(MFX_FOURCC_NV12);
    params.mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
    params.mfx.FrameInfo.Shift = 0;
    params.mfx.FrameInfo.CropX = 0;
    params.mfx.FrameInfo.CropY = 0;
    params.mfx.FrameInfo.CropW = static_cast<mfxU16>(width_);
    params.mfx.FrameInfo.CropH = static_cast<mfxU16>(height_);
    params.mfx.FrameInfo.Width = MSDK_ALIGN16(static_cast<mfxU16>(width_));
    params.mfx.FrameInfo.Height = MSDK_ALIGN16(static_cast<mfxU16>(height_));
    params.AsyncDepth = 1;
    enc_coding_opt_.PicTimingSEI = MFX_CODINGOPTION_OFF;
    enc_coding_opt_.NalHrdConformance = MFX_CODINGOPTION_OFF;
    enc_coding_opt_.VuiNalHrdParameters = MFX_CODINGOPTION_OFF;
    enc_coding_opt_.Header.BufferId = MFX_EXTBUFF_CODING_OPTION;
    enc_coding_opt_.Header.BufferSz = sizeof(mfxExtCodingOption);
    enc_ext_buffers_.push_back(reinterpret_cast<mfxExtBuffer*>(&enc_coding_opt_));
    params.ExtParam = enc_ext_buffers_.data();
    params.NumExtParam = 1;
    return params;
}

mfxVideoParam IntelEncoderImpl::genVppParams(const VplParamsHelper& params_helper) {
    mfxVideoParam params{};
    params.IOPattern = MFX_IOPATTERN_IN_VIDEO_MEMORY | MFX_IOPATTERN_OUT_VIDEO_MEMORY;
    params.vpp.In.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
    ConvertFrameRate(params_helper.fps(), &params.mfx.FrameInfo.FrameRateExtN,
                     &params.mfx.FrameInfo.FrameRateExtD);
    params.vpp.In.AspectRatioW = 1;
    params.vpp.In.AspectRatioH = 1;
    params.vpp.In.FourCC = MFX_FOURCC_RGB4;
    params.vpp.In.ChromaFormat = FourCCToChroma(MFX_FOURCC_RGB4);
    params.vpp.In.CropX = 0;
    params.vpp.In.CropY = 0;
    params.vpp.In.CropW = static_cast<mfxU16>(width_);
    params.vpp.In.CropH = static_cast<mfxU16>(height_);
    params.vpp.In.Width = MSDK_ALIGN16(static_cast<mfxU16>(width_));
    params.vpp.In.Height = MSDK_ALIGN16(static_cast<mfxU16>(height_));
    params.vpp.In.Shift = 0;
    // Output data
    memcpy(&params.vpp.Out, &params.vpp.In, sizeof(params.vpp.In));
    params.vpp.Out.FourCC = MFX_FOURCC_NV12;
    params.vpp.Out.ChromaFormat = FourCCToChroma(MFX_FOURCC_NV12);
    params.AsyncDepth = 1;
    vpp_signal_.Header.BufferId = MFX_EXTBUFF_VPP_VIDEO_SIGNAL_INFO;
    vpp_signal_.Header.BufferSz = sizeof(mfxExtVPPVideoSignalInfo);
    vpp_signal_.In.NominalRange = MFX_NOMINALRANGE_0_255;
    vpp_signal_.Out.NominalRange = MFX_NOMINALRANGE_16_235;
    // TransferMatrix描述一个转换过程，为什么还要区分In和Out，我无法理解
    vpp_signal_.In.TransferMatrix = MFX_TRANSFERMATRIX_BT709;
    vpp_signal_.Out.TransferMatrix = MFX_TRANSFERMATRIX_BT709;
    vpp_ext_buffers_.push_back(reinterpret_cast<mfxExtBuffer*>(&vpp_signal_));
    params.ExtParam = vpp_ext_buffers_.data();
    params.NumExtParam = 1;
    return params;
}

IntelEncoder::IntelEncoder(void* d3d11_dev, void* d3d11_ctx, int64_t luid, uint32_t width,
                           uint32_t height)
    : VideoEncoder{d3d11_dev, d3d11_ctx, width, height}
    , impl_{std::make_shared<IntelEncoderImpl>(reinterpret_cast<ID3D11Device*>(d3d11_dev),
                                               reinterpret_cast<ID3D11DeviceContext*>(d3d11_ctx),
                                               luid)} {}

bool IntelEncoder::init(const VideoEncodeParamsHelper& params) {
    return impl_->init(params);
}

void IntelEncoder::reconfigure(const ReconfigureParams& params) {
    impl_->reconfigure(params);
}

std::shared_ptr<ltproto::peer2peer::VideoFrame> IntelEncoder::encodeFrame(void* input_frame) {
    return impl_->encodeOneFrame(input_frame, needKeyframe());
}

} // namespace lt

#undef MSDK_ALIGN32
#undef MSDK_ALIGN16