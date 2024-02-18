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

#include <ltlib/logging.h>

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
    VplParamsHelper(const lt::video::EncodeParamsHelper& params)
        : params_{params} {}

    uint32_t width() const { return params_.width(); }
    uint32_t height() const { return params_.height(); }
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
    void set_bitrate(uint32_t bps) { params_.set_bitrate(bps); }
    void set_fps(int f) { params_.set_fps(f); }

private:
    lt::video::EncodeParamsHelper params_;
};

mfxU32 VplParamsHelper::codec() const {
    return params_.codec() == lt::VideoCodecType::H264 ? MFX_CODEC_AVC : MFX_CODEC_HEVC;
}

mfxU16 VplParamsHelper::rc() const {
    switch (params_.rc()) {
    case lt::video::EncodeParamsHelper::RcMode::CBR:
        return MFX_RATECONTROL_CBR;
    case lt::video::EncodeParamsHelper::RcMode::VBR:
        return MFX_RATECONTROL_VBR;
    default:
        assert(false);
        return MFX_RATECONTROL_VBR;
    }
}

mfxU16 VplParamsHelper::preset() const {
    switch (params_.preset()) {
    case lt::video::EncodeParamsHelper::Preset::Balanced:
        return MFX_TARGETUSAGE_BALANCED;
    case lt::video::EncodeParamsHelper::Preset::Speed:
        return MFX_TARGETUSAGE_BEST_SPEED;
    case lt::video::EncodeParamsHelper::Preset::Quality:
        return MFX_TARGETUSAGE_BEST_QUALITY;
    default:
        assert(false);
        return MFX_TARGETUSAGE_UNKNOWN;
    }
}

mfxU16 VplParamsHelper::profile() const {
    switch (params_.profile()) {
    case lt::video::EncodeParamsHelper::Profile::AvcMain:
        return MFX_PROFILE_AVC_MAIN;
    case lt::video::EncodeParamsHelper::Profile::HevcMain:
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

namespace video {

class IntelEncoderImpl {
public:
    IntelEncoderImpl(const EncodeParamsHelper& params);
    ~IntelEncoderImpl();
    bool init();
    void reconfigure(const Encoder::ReconfigureParams& params);
    uint32_t width() const { return params_.width(); }
    uint32_t height() const { return params_.height(); }
    VideoCodecType codecType() const;
    std::shared_ptr<ltproto::client2worker::VideoFrame> encodeOneFrame(void* input_frame,
                                                                       bool request_iframe);

private:
    bool createMfxSession();
    bool setConfigFilter();
    bool findImplIndex();
    void printAllImpls();
    bool initEncoder(const VplParamsHelper& params_helper);

    ComPtr<ID3D11Texture2D> allocEncodeTexture();
    mfxVideoParam genEncodeParams(const VplParamsHelper& params_helper);


private:
    ComPtr<ID3D11Device> d3d11_dev_;
    ComPtr<ID3D11DeviceContext> d3d11_ctx_;
    ComPtr<ID3D11Texture2D> encode_texture_;
    const int64_t luid_;
    int32_t impl_index_ = -1;

    lt::VideoCodecType codec_type_;
    mfxLoader mfxloader_ = nullptr;
    mfxSession mfxsession_ = nullptr;
    mfxVideoParam encode_param_{};
    mfxVideoParam vpp_param_{};
    std::unique_ptr<MfxEncoderFrameAllocator> allocator_;
    mfxExtVPPVideoSignalInfo vpp_signal_{};
    std::vector<mfxExtBuffer*> vpp_ext_buffers_;

    std::vector<mfxExtBuffer*> enc_ext_buffers_;
    std::vector<uint8_t> bitstream_;
    VplParamsHelper params_;
};

IntelEncoderImpl::IntelEncoderImpl(const EncodeParamsHelper& params)
    : d3d11_dev_{reinterpret_cast<ID3D11Device*>(params.d3d11_dev())}
    , d3d11_ctx_{reinterpret_cast<ID3D11DeviceContext*>(params.d3d11_ctx())}
    , luid_{params.luid()}
    , codec_type_{params.codec()}
    , params_{params} {}

IntelEncoderImpl::~IntelEncoderImpl() {
    if (mfxloader_ != nullptr) {
        MFXUnload(mfxloader_);
    }
}

bool IntelEncoderImpl::init() {
    Microsoft::WRL::ComPtr<ID3D10Multithread> tmp10;
    auto hr = d3d11_ctx_.As(&tmp10);
    if (FAILED(hr)) {
        LOG(ERR) << "Cast to ID3D10Multithread failed with " << GetLastError();
        return false;
    }
    tmp10->SetMultithreadProtected(true);
    // 我无法理解设计出这个“allocator”的人是怎么想的
    allocator_ = std::make_unique<MfxEncoderFrameAllocator>(d3d11_dev_, d3d11_ctx_);
    mfxloader_ = MFXLoad();
    if (mfxloader_ == nullptr) {
        LOG(ERR) << "MFXLoad failed";
        return false;
    }
    if (!createMfxSession()) {
        return false;
    }
    mfxStatus status =
        MFXVideoCORE_SetHandle(mfxsession_, MFX_HANDLE_D3D11_DEVICE, d3d11_dev_.Get());
    if (status != MFX_ERR_NONE) {
        LOGF(ERR, "MFXVideoCORE_SetHandle(MFX_HANDLE_D3D11_DEVICE, %p) failed with %d",
             d3d11_dev_.Get(), status);
        return false;
    }
    status = MFXVideoCORE_SetFrameAllocator(mfxsession_, allocator_.get());
    if (status != MFX_ERR_NONE) {
        LOG(ERR) << "MFXVideoCORE_SetFrameAllocator failed with " << status;
        return false;
    }

    if (!initEncoder(params_)) {
        return false;
    }
    return true;
}

void IntelEncoderImpl::reconfigure(const Encoder::ReconfigureParams& params) {
    if (!params.bitrate_bps.has_value() && !params.fps.has_value()) {
        return;
    }
    if (params.bitrate_bps.has_value()) {
        params_.set_bitrate(params.bitrate_bps.value());
        VplSize vsize;
        // uint32_t old8 = static_cast<uint32_t>(static_cast<uint32_t>(encode_param_.mfx.TargetKbps)
        //                                      encode_param_.mfx.BRCParamMultiplier * 0.8);
        // vsize.target = std::max(old8, target_kbps);
        vsize.target = params_.bitrate_kbps();
        vsize.max = params_.maxbitrate_kbps();
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
        params_.set_fps(params.fps.value());
        ConvertFrameRate(params.fps.value(), &encode_param_.mfx.FrameInfo.FrameRateExtN,
                         &encode_param_.mfx.FrameInfo.FrameRateExtD);
    }
    // LOG(INFO) << "before query";
    // printMfxVideoParamEncode(encode_param_);
    mfxStatus status = MFXVideoENCODE_Query(mfxsession_, &encode_param_, &encode_param_);
    if (status > MFX_ERR_NONE) {
        LOG(ERR) << "MFXVideoENCODE_Query invalid parameters";
        // 是合法的留下？？？
        // LOG(INFO) << "after query";
        // printMfxVideoParamEncode(encode_param_);
        status = MFXVideoENCODE_Query(mfxsession_, &encode_param_, &encode_param_);
        // LOG(INFO) << "after query2";
        // printMfxVideoParamEncode(encode_param_);
    }
    if (status < MFX_ERR_NONE) {
        LOG(ERR) << "MFXVideoENCODE_Query failed";
        return;
    }
    status = MFXVideoENCODE_Reset(mfxsession_, &encode_param_);
    // LOG(INFO) << "after reset";
    // printMfxVideoParamEncode(encode_param_);
    if (status != MFX_ERR_NONE) {
        LOG(ERR) << "MFXVideoENCODE_Reset failed with " << status;
    }
}

VideoCodecType IntelEncoderImpl::codecType() const {
    return codec_type_;
}

std::shared_ptr<ltproto::client2worker::VideoFrame>
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
        status = MFXVideoENCODE_EncodeFrameAsync(mfxsession_, pctrl,
                                                 &vpp_surface /*encode_surface*/, &bs, &sync_point);
        if (status == MFX_WRN_DEVICE_BUSY) {
            std::this_thread::sleep_for(std::chrono::milliseconds{1});
            continue;
        }
        else if (status >= MFX_ERR_NONE) {
            break;
        }
        else if (status == MFX_ERR_NOT_ENOUGH_BUFFER) {
            LOG(ERR) << "MFXVideoENCODE_EncodeFrameAsync failed with MFX_ERR_NOT_ENOUGH_BUFFER";
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
    auto out_frame = std::make_shared<ltproto::client2worker::VideoFrame>();
    out_frame->set_frame(bitstream_.data(), bs.DataLength);
    out_frame->set_is_keyframe(is_keyframe);
    return out_frame;
}

bool IntelEncoderImpl::createMfxSession() {
    if (!setConfigFilter()) {
        return false;
    }
    // printAllImpls();
    if (!findImplIndex()) {
        return false;
    }
    mfxStatus status = MFXCreateSession(mfxloader_, impl_index_, &mfxsession_);
    if (status != MFX_ERR_NONE) {
        LOG(ERR) << "MFXCreateSession failed with " << status;
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
        LOG(ERR) << "MFXSetConfigFilterProperty(mfxImplDescription.Impl=MFX_IMPL_TYPE_HARDWARE)"
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
        LOG(ERR) << "MFXSetConfigFilterProperty(mfxImplDescription.AccelerationMode=MFX_ACCEL_"
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

void IntelEncoderImpl::printAllImpls() {
    mfxStatus status = MFX_ERR_NONE;
    mfxImplDescription* desc = nullptr;
    for (int index = 0; status == MFX_ERR_NONE; index++) {
        if (desc != nullptr) {
            MFXDispReleaseImplDescription(mfxloader_, desc);
            desc = nullptr;
        }
        status = MFXEnumImplementations(mfxloader_, index, MFX_IMPLCAPS_IMPLDESCSTRUCTURE,
                                        (mfxHDL*)&desc);
        if (status != MFX_ERR_NONE || desc == nullptr) {
            continue;
        }
        LOGF(INFO,
             "MFXImpl index:%d, impl:%d, accemode:%d, apiver:%u, api.major:%u, api.minor:%u, "
             "name:%s, license:%s, keywords:%s, vendor:%#x, vendorimpl:%u",
             index, desc->Impl, desc->AccelerationMode, desc->ApiVersion.Version,
             desc->ApiVersion.Major, desc->ApiVersion.Minor, desc->ImplName, desc->License,
             desc->Keywords, desc->VendorID, desc->VendorImplID);
    }
    if (desc != nullptr) {
        MFXDispReleaseImplDescription(mfxloader_, desc);
    }
}

bool IntelEncoderImpl::initEncoder(const VplParamsHelper& params_helper) {
    auto params = genEncodeParams(params_helper);
    // LOG(INFO) << "Generated VPL encode params";
    // printMfxVideoParamEncode(params);
    mfxStatus status = MFX_ERR_NONE;
    status = MFXVideoENCODE_Init(mfxsession_, &params);
    if (status < MFX_ERR_NONE) { // FIXME: 这里会有参数不兼容的警告
        LOG(ERR) << "MFXVideoENCODE_Init failed with " << status;
        return false;
    }
    else if (status > MFX_ERR_NONE) {
        LOG(ERR) << "MFXVideoENCODE_Init warn " << status;
    }
    mfxVideoParam param_out{};
    status = MFXVideoENCODE_GetVideoParam(mfxsession_, &param_out);
    if (status != MFX_ERR_NONE) {
        LOG(ERR) << "MFXVideoENCODE_GetVideoParam failed with " << status;
        return false;
    }
    // LOG(INFO) << "GetVideoParam encode";
    // printMfxVideoParamEncode(param_out);
    encode_param_ = param_out;
    return true;
}



Microsoft::WRL::ComPtr<ID3D11Texture2D> IntelEncoderImpl::allocEncodeTexture() {
    Microsoft::WRL::ComPtr<ID3D11Texture2D> frame;
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = MSDK_ALIGN16(static_cast<mfxU16>(params_.width()));
    desc.Height = MSDK_ALIGN16(static_cast<mfxU16>(params_.height()));
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
        LOG(ERR) << "D3D11Device::CreateTexture2D failed with " << GetLastError();
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
    if (isAVC(codec_type_)) {
        params.mfx.IdrInterval = 0;
        params.mfx.NumSlice = 1;
    }
    else {
        params.mfx.IdrInterval = 1;
        params.mfx.NumSlice = 0;
    }

    params.mfx.CodecProfile = params_helper.profile();
    params.mfx.CodecLevel = 0; // 未填
    params.mfx.MaxKbps = static_cast<mfxU16>(vsize.max);
    params.mfx.InitialDelayInKB = static_cast<mfxU16>(vsize.init_delay);
    params.mfx.GopOptFlag = MFX_GOP_CLOSED;
    params.mfx.BufferSizeInKB = static_cast<mfxU16>(vsize.buffer_size);

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
    params.mfx.FrameInfo.CropW = static_cast<mfxU16>(params_.width());
    params.mfx.FrameInfo.CropH = static_cast<mfxU16>(params_.height());
    params.mfx.FrameInfo.Width = MSDK_ALIGN16(static_cast<mfxU16>(params_.width()));
    params.mfx.FrameInfo.Height = MSDK_ALIGN16(static_cast<mfxU16>(params_.height()));
    params.AsyncDepth = 1;

    return params;
}

std::unique_ptr<IntelEncoder> IntelEncoder::create(const EncodeParamsHelper& params) {
    auto encoder = std::make_unique<IntelEncoder>();
    auto impl = std::make_shared<IntelEncoderImpl>(params);
    if (!impl->init()) {
        return nullptr;
    }
    encoder->impl_ = impl;
    return encoder;
}

void IntelEncoder::reconfigure(const ReconfigureParams& params) {
    impl_->reconfigure(params);
}

CaptureFormat IntelEncoder::captureFormat() const {
    return CaptureFormat::D3D11_BGRA;
}

VideoCodecType IntelEncoder::codecType() const {
    return impl_->codecType();
}

uint32_t IntelEncoder::width() const {
    return impl_->width();
}

uint32_t IntelEncoder::height() const {
    return impl_->height();
}

std::shared_ptr<ltproto::client2worker::VideoFrame> IntelEncoder::encodeFrame(void* input_frame) {
    return impl_->encodeOneFrame(input_frame, needKeyframe());
}

} // namespace video

} // namespace lt

#undef MSDK_ALIGN32
#undef MSDK_ALIGN16
