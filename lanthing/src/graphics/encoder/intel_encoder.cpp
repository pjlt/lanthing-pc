#define ONEVPL_EXPERIMENTAL 1
#include "intel_encoder.h"
#include "intel_allocator.h"

#include <d3d11.h>
#include <wrl/client.h>

#include <vpl/mfx.h>

#include <g3log/g3log.hpp>

/***
 * MediaSDK/oneVPLAPI大概只有内部人员用得明白吧😅
 * API复杂、文档不清不楚，无奈看“源码”，发现仅仅是加载了驱动提供的函数
 * 😅😅😅😅😅😅😅😅😅😅😅😅😅😅😅😅😅😅😅😅😅😅😅😅
 ***/

#define MSDK_ALIGN16(value) (((value + 15) >> 4) << 4)
#define MSDK_ALIGN32(X) (((mfxU32)((X) + 31)) & (~(mfxU32)31))

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
    return params_.codec() == rtc::VideoCodecType::H264 ? MFX_CODEC_AVC : MFX_CODEC_HEVC;
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

} // namespace

namespace lt {

class IntelEncoderImpl {
public:
    IntelEncoderImpl(ID3D11Device* d3d11_dev, ID3D11DeviceContext* d3d11_ctx, int64_t luid);
    ~IntelEncoderImpl();
    bool init(const VideoEncodeParamsHelper& params);
    void reconfigure(const VideoEncoder::ReconfigureParams& params);
    VideoEncoder::EncodedFrame encodeOneFrame(void* input_frame, bool request_iframe);

private:
    bool createMfxSession();
    bool setConfigFilter();
    bool findImplIndex();
    bool initEncoder(const VplParamsHelper& params_helper);
    // bool initVpp(const VplParamsHelper& params_helper);
    // Microsoft::WRL::ComPtr<ID3D11Texture2D> allocRenderSurface();
    mfxVideoParam genEncodeParams(const VplParamsHelper& params_helper);
    // mfxVideoParam genVppParams(const VplParamsHelper& params_helper);

private:
    Microsoft::WRL::ComPtr<ID3D11Device> d3d11_dev_;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> d3d11_ctx_;
    const int64_t luid_;
    int32_t impl_index_ = -1;
    uint32_t width_;
    uint32_t height_;
    rtc::VideoCodecType codec_type_;
    mfxLoader mfxloader_ = nullptr;
    mfxSession mfxsession_ = nullptr;
    mfxVideoParam encode_param_{};
    // mfxVideoParam vpp_param_{};
    const mfxU32 fourcc_ = MFX_FOURCC_RGB4;
    std::unique_ptr<MfxEncoderFrameAllocator> allocator_;
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
    mfxStatus status = MFXVideoCORE_SetHandle(mfxsession_, MFX_HANDLE_D3D11_DEVICE, d3d11_dev_.Get());
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
    // if (!initVpp(params_helper)) {
    //     return false;
    // }
    return true;
}

void IntelEncoderImpl::reconfigure(const VideoEncoder::ReconfigureParams& params) {
    if (!params.bitrate_bps.has_value() && !params.fps.has_value()) {
        return;
    }
    if (params.bitrate_bps.has_value()) {
        encode_param_.mfx.TargetKbps = static_cast<mfxU16>(params.bitrate_bps.value() / 1000);
        encode_param_.mfx.MaxKbps = static_cast<mfxU16>(encode_param_.mfx.TargetKbps * 1.05f);
    }
    if (params.fps.has_value()) {
        ConvertFrameRate(params.fps.value(), &encode_param_.mfx.FrameInfo.FrameRateExtN,
                         &encode_param_.mfx.FrameInfo.FrameRateExtD);
    }
    mfxStatus status = MFXVideoENCODE_Query(mfxsession_, &encode_param_, &encode_param_);
    if (status > MFX_ERR_NONE) {
        LOG(WARNING) << "MFXVideoENCODE_Query invalid parameters";
        // 是合法的留下？？？
        status = MFXVideoENCODE_Query(mfxsession_, &encode_param_, &encode_param_);
    }
    if (status < MFX_ERR_NONE) {
        LOG(WARNING) << "MFXVideoENCODE_Query failed";
        return;
    }
    status = MFXVideoENCODE_Reset(mfxsession_, &encode_param_);
    if (status != MFX_ERR_NONE) {
        LOG(WARNING) << "MFXVideoENCODE_Reset failed with " << status;
    }
}

VideoEncoder::EncodedFrame IntelEncoderImpl::encodeOneFrame(void* input_frame,
                                                               bool request_iframe) {
    VideoEncoder::EncodedFrame out_frame{};
    mfxSyncPoint sync_point{};
    std::shared_ptr<uint8_t> buffer(new uint8_t[encode_param_.mfx.BufferSizeInKB * 1000]);
    mfxBitstream bs{};
    bs.Data = buffer.get();
    bs.MaxLength = encode_param_.mfx.BufferSizeInKB * 1000;
    mfxEncodeCtrl ctrl{};
    mfxEncodeCtrl* pctrl = nullptr;
    if (request_iframe) {
        // 这个FrameType应该填单个类型还是或起来？
        ctrl.FrameType = MFX_FRAMETYPE_I | MFX_FRAMETYPE_IDR;
        pctrl = &ctrl;
    }
    mfxFrameSurface1 surface{};
    surface.Data.MemId = input_frame;
    surface.Info = encode_param_.mfx.FrameInfo;
    mfxStatus status = MFX_ERR_NONE;
    while (true) {
        status = MFXVideoENCODE_EncodeFrameAsync(mfxsession_, pctrl, &surface, &bs, &sync_point);
        if (status == MFX_WRN_DEVICE_BUSY) {
            std::this_thread::sleep_for(std::chrono::milliseconds{1});
            continue;
        }
        else if (status >= MFX_ERR_NONE) {
            break;
        }
        else if (status == MFX_ERR_NOT_ENOUGH_BUFFER) {
            LOG(WARNING) << "MFXVideoENCODE_EncodeFrameAsync failed with MFX_ERR_NOT_ENOUGH_BUFFER";
            assert(fasle);
            break;
        }
        else {
            return out_frame;
        }
    }
    status = MFX_WRN_IN_EXECUTION;
    while (status > 0) {
        status = MFXVideoCORE_SyncOperation(mfxsession_, sync_point, 2000);
        if (status == MFX_ERR_NONE) {
            break;
        }
        else if (status < MFX_ERR_NONE) {
            return out_frame;
        }
        else {
            std::this_thread::sleep_for(std::chrono::milliseconds{1});
        }
    }
    out_frame.internal_data = buffer;
    out_frame.data = buffer.get();
    out_frame.size = bs.DataLength;
    out_frame.width = width_;
    out_frame.height = height_;
    out_frame.is_keyframe = (bs.FrameType & MFX_FRAMETYPE_I) || (bs.FrameType & MFX_FRAMETYPE_IDR);
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
    mfxStatus status = MFX_ERR_NONE;
    status = MFXVideoENCODE_Init(mfxsession_, &params);
    if (status != MFX_ERR_NONE) {
        LOG(WARNING) << "MFXVideoENCODE_Init failed with " << status;
        return false;
    }
    mfxVideoParam param_out{};
    status = MFXVideoENCODE_GetVideoParam(mfxsession_, &param_out);
    if (status != MFX_ERR_NONE) {
        LOG(WARNING) << "MFXVideoENCODE_GetVideoParam failed with " << status;
        return false;
    }
    encode_param_ = param_out;
    return true;
}

// bool IntelEncoderImpl::initVpp(const VplParamsHelper& params_helper) {
//     mfxVideoParam params = genVppParams(params_helper);
//     render_surface_ = allocRenderSurface();
//     mfxStatus status = MFXVideoVPP_Init(mfxsession_, &params);
//     if (status != MFX_ERR_NONE) {
//         LOG(WARNING) << "MFXVideoVPP_Init failed with " << status;
//         return false;
//     }
//     mfxVideoParam param_out{};
//     status = MFXVideoVPP_GetVideoParam(mfxsession_, &param_out);
//     if (status != MFX_ERR_NONE) {
//         LOG(WARNING) << "MFXVideoVPP_GetVideoParam failed with " << status;
//         return false;
//     }
//     vpp_param_ = param_out;
//     return true;
// }

// Microsoft::WRL::ComPtr<ID3D11Texture2D> IntelEncoderImpl::allocRenderSurface() {
//     Microsoft::WRL::ComPtr<ID3D11Texture2D> frame;
//     D3D11_TEXTURE2D_DESC desc{};
//     desc.Width = MSDK_ALIGN16(static_cast<mfxU16>(width_));
//     desc.Height = MSDK_ALIGN32(static_cast<mfxU16>(height_));
//     desc.MipLevels = 1;
//     desc.ArraySize = 1;
//     desc.SampleDesc.Count = 1;
//     desc.Usage = D3D11_USAGE_DEFAULT;
//     desc.Format = DXGI_FORMAT_NV12; // ??
//     desc.BindFlags = D3D11_BIND_RENDER_TARGET;
//     desc.MiscFlags = 0;
//     // desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
//     HRESULT hr = d3d11_dev_->CreateTexture2D(&desc, nullptr, frame.GetAddressOf());
//     if (FAILED(hr)) {
//         // assert(false);
//         LOG(WARNING) << "D3D11Device::CreateTexture2D failed with " << GetLastError();
//         return nullptr;
//     }
//     return frame;
// }

mfxVideoParam IntelEncoderImpl::genEncodeParams(const VplParamsHelper& params_helper) {
    mfxVideoParam params{};
    params.mfx.CodecId = params_helper.codec();
    params.mfx.TargetUsage = params_helper.preset();
    params.mfx.TargetKbps = params_helper.bitrate_kbps();
    params.mfx.RateControlMethod = params_helper.rc();
    params.mfx.GopRefDist = 1;
    params.mfx.GopPicSize = static_cast<mfxU16>(1000000);
    params.mfx.NumRefFrame = 1;
    params.mfx.IdrInterval = 0; // 未填
    params.mfx.CodecProfile = params_helper.profile();
    params.mfx.CodecLevel = 0; // 未填
    params.mfx.MaxKbps = params_helper.maxbitrate_kbps();
    params.mfx.InitialDelayInKB = 0; // 未填
    params.mfx.GopOptFlag = 0;       // 未填
    params.mfx.BufferSizeInKB = 512;
    params.mfx.NumSlice = 0; // 未填值
    params.mfx.EncodedOrder = 0;
    params.IOPattern = MFX_IOPATTERN_IN_VIDEO_MEMORY;

    ConvertFrameRate(params_helper.fps(), &params.mfx.FrameInfo.FrameRateExtN,
                     &params.mfx.FrameInfo.FrameRateExtD);

    params.mfx.FrameInfo.FourCC = fourcc_;
    params.mfx.FrameInfo.ChromaFormat = FourCCToChroma(fourcc_);
    params.mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
    params.mfx.FrameInfo.Shift = 0;
    params.mfx.FrameInfo.CropX = 0;
    params.mfx.FrameInfo.CropY = 0;
    params.mfx.FrameInfo.CropW = static_cast<mfxU16>(width_);
    params.mfx.FrameInfo.CropH = static_cast<mfxU16>(height_);
    params.mfx.FrameInfo.Width = MSDK_ALIGN16(static_cast<mfxU16>(width_));
    params.mfx.FrameInfo.Height = MSDK_ALIGN32(static_cast<mfxU16>(height_));
    params.AsyncDepth = 1;
    return params;
}

// mfxVideoParam IntelEncoderImpl::genVppParams(const VplParamsHelper& params_helper) {
//     mfxVideoParam params{};
//     params.IOPattern = MFX_IOPATTERN_IN_VIDEO_MEMORY | MFX_IOPATTERN_OUT_VIDEO_MEMORY;
//     // Input data
//     params.vpp.In.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
//     ConvertFrameRate(params_helper.fps(), &params.mfx.FrameInfo.FrameRateExtN,
//                      &params.mfx.FrameInfo.FrameRateExtD);
//     params.vpp.In.AspectRatioW = 1;
//     params.vpp.In.AspectRatioH = 1;
//     params.vpp.In.FourCC = fourcc_;
//     params.vpp.In.ChromaFormat = FourCCToChroma(params.vpp.In.fourcc_);
//     params.vpp.In.CropX = 0;
//     params.vpp.In.CropY = 0;
//     params.vpp.In.CropW = static_cast<mfxU16>(width_);
//     params.vpp.In.CropH = static_cast<mfxU16>(height_);
//     params.vpp.In.Width = MSDK_ALIGN16(static_cast<mfxU16>(width_));
//     params.vpp.In.Height = MSDK_ALIGN16(static_cast<mfxU16>(height_));
//     params.vpp.In.Shift = 0;
//     // Output data
//     memcpy(&params.vpp.Out, &params.vpp.In, sizeof(params.vpp.In));
// }

IntelEncoder::IntelEncoder(void* d3d11_dev, void* d3d11_ctx, int64_t luid)
    : VideoEncoder{d3d11_dev, d3d11_ctx}
    , impl_{std::make_shared<IntelEncoderImpl>(
          reinterpret_cast<ID3D11Device*>(d3d11_dev),
          reinterpret_cast<ID3D11DeviceContext*>(d3d11_ctx), luid)} {}

bool IntelEncoder::init(const VideoEncodeParamsHelper& params) {
    return impl_->init(params);
}

void IntelEncoder::reconfigure(const ReconfigureParams& params) {
    impl_->reconfigure(params);
}

VideoEncoder::EncodedFrame IntelEncoder::encode_one_frame(void* input_frame, bool force_idr) {
    return impl_->encodeOneFrame(input_frame, force_idr);
}

} // namespace lt

#undef MSDK_ALIGN32
#undef MSDK_ALIGN16