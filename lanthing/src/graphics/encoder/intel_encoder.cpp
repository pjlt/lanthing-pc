#include "intel_encoder.h"
#include "intel_allocator.h"

#include <array>
#include <thread>

#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>

#include <mfxvideo.h>

#include <g3log/g3log.hpp>

#include <ltlib/times.h>

// https://github.com/Intel-Media-SDK/samples/blob/master/samples/sample_encode/src/pipeline_region_encode.cpp

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

} // namespace

namespace lt {

class IntelEncoderImpl {
public:
    IntelEncoderImpl(ID3D11Device* d3d11_dev);
    ~IntelEncoderImpl() = default;
    bool init(const VideoEncoder::InitParams& params);
    void reconfigure(const VideoEncoder::ReconfigureParams& params);
    VideoEncoder::EncodedFrame encode_one_frame(void* input_frame, bool force_idr);

private:
    bool init_encoder();
    bool init_vpp();
    Microsoft::WRL::ComPtr<ID3D11Texture2D> alloc_render_surface();

    mfxVideoParam gen_encode_param();
    mfxVideoParam gen_vpp_param();

private:
    uint32_t width_ = 0;
    uint32_t height_ = 0;
    uint32_t bitrate_bps_ = 0;
    rtc::VideoCodecType codec_type_ = rtc::VideoCodecType::Unknown;
    std::shared_ptr<MfxEncoderFrameAllocator> allocator_;
    mfxVideoParam encode_param_;
    mfxVideoParam vpp_param_;
    mfxSession mfx_session_ = nullptr;
    Microsoft::WRL::ComPtr<IDXGIFactory2> factory_;
    Microsoft::WRL::ComPtr<ID3D11Device> device_;
    Microsoft::WRL::ComPtr<IDXGIAdapter> adapter_;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> device_context_;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> render_surface_;
    bool enable_qsvff_ = false;
};

IntelEncoder::IntelEncoder(void* d3d11_dev, void* d3d11_ctx)
    : VideoEncoder{d3d11_dev, d3d11_ctx}
    , impl_(std::make_shared<IntelEncoderImpl>(reinterpret_cast<ID3D11Device*>(d3d11_dev))) {}

bool IntelEncoder::init(const InitParams& params) {
    return impl_->init(params);
}

void IntelEncoder::reconfigure(const ReconfigureParams& params) {
    impl_->reconfigure(params);
}

VideoEncoder::EncodedFrame IntelEncoder::encode_one_frame(void* input_frame, bool force_idr) {
    return impl_->encode_one_frame(input_frame, force_idr);
}

IntelEncoderImpl::IntelEncoderImpl(ID3D11Device* d3d11_dev)
    : device_{reinterpret_cast<ID3D11Device*>(d3d11_dev)} {}

bool IntelEncoderImpl::init(const VideoEncoder::InitParams& params) {
    if (!params.validate()) {
        return false;
    }
    width_ = params.width;
    height_ = params.height;
    device_->GetImmediateContext(device_context_.GetAddressOf());
    codec_type_ = params.codec_type;
    bitrate_bps_ = params.bitrate_bps;
    allocator_ = std::make_shared<MfxEncoderFrameAllocator>(device_, device_context_);
    Microsoft::WRL::ComPtr<ID3D10Multithread> tmp10;
    auto hr = device_context_.As(&tmp10);
    if (FAILED(hr)) {
        // LOG(WARNING) << "Cast to ID3D10Multithread failed";
        return false;
    }
    tmp10->SetMultithreadProtected(true);
    mfxVersion ver = {{0, 1}};
    // FIXEME set impl to previous impl
    mfxIMPL impl = MFX_IMPL_HARDWARE_ANY | MFX_IMPL_VIA_D3D11;
    mfxStatus status = MFXInit(impl, &ver, &mfx_session_);
    if (status != MFX_ERR_NONE) {
        mfx_session_ = nullptr;
        return false;
    }
    MFXQueryIMPL(mfx_session_, &impl);
    MFXQueryVersion(mfx_session_, &ver);
    mfxPlatform platform{};
    MFXVideoCORE_QueryPlatform(mfx_session_, &platform);
    if ((codec_type_ == rtc::VideoCodecType::H264 && platform.CodeName > MFX_PLATFORM_SKYLAKE) ||
        (codec_type_ == rtc::VideoCodecType::H265 && platform.CodeName > MFX_PLATFORM_ICELAKE)) {
        enable_qsvff_ = true;
    }
    status = MFXVideoCORE_SetHandle(mfx_session_, MFX_HANDLE_D3D11_DEVICE, device_.Get());
    if (status != MFX_ERR_NONE) {
        return false;
    }
    status = MFXVideoCORE_SetFrameAllocator(mfx_session_, allocator_.get());
    if (status != MFX_ERR_NONE) {
        return false;
    }
    if (!init_encoder()) {
        return false;
    }
    if (!init_vpp()) {
        return false;
    }
    return true;
}

void IntelEncoderImpl::reconfigure(const VideoEncoder::ReconfigureParams& params) {
    (void)params;
    // todo: 重置编码器
}

VideoEncoder::EncodedFrame IntelEncoderImpl::encode_one_frame(void* input_frame, bool force_idr) {
    (void)force_idr; // TODO: 请求I帧
    VideoEncoder::EncodedFrame out_frame;
    mfxFrameSurface1 vppin, vppout;
    memset(&vppin, 0, sizeof(mfxFrameSurface1));
    memset(&vppout, 0, sizeof(mfxFrameSurface1));
    mfxBitstream bs;
    mfxSyncPoint syncp_encode; //, syncp_vpp;
    std::shared_ptr<uint8_t> buffer(new uint8_t[encode_param_.mfx.BufferSizeInKB * 1000]);
    memset(&bs, 0, sizeof(bs));
    bs.Data = buffer.get();
    bs.MaxLength = encode_param_.mfx.BufferSizeInKB * 1000;

    vppin.Data.MemId = input_frame;
    vppin.Info = vpp_param_.vpp.In;

    vppout.Data.MemId = render_surface_.Get();
    vppout.Info = vpp_param_.vpp.Out;
    mfxStatus status;
    // while (true) {
    //     status = MFXVideoVPP_RunFrameVPPAsync(mfx_session_, &vppin, &vppout, nullptr,
    //     &syncp_vpp); if (status == MFX_WRN_DEVICE_BUSY) {
    //         std::this_thread::sleep_for(std::chrono::milliseconds { 1 });
    //         continue;
    //     } else if (status >= MFX_ERR_NONE || status == MFX_ERR_MORE_DATA || status ==
    //     MFX_ERR_MORE_SURFACE) {
    //         break;
    //     } else {
    //         return out_frame;
    //     }
    // }
    while (true) {
        status = MFXVideoENCODE_EncodeFrameAsync(mfx_session_, nullptr, &vppin, &bs, &syncp_encode);
        if (status == MFX_WRN_DEVICE_BUSY) {
            std::this_thread::sleep_for(std::chrono::milliseconds{1});
            continue;
        }
        else if (status >= MFX_ERR_NONE) {
            break;
        }
        else if (status == MFX_ERR_NOT_ENOUGH_BUFFER) {
            // TODO: error
            break;
        }
        else {
            return out_frame;
        }
    }
    status = MFX_WRN_IN_EXECUTION;
    while (status > 0) {
        status = MFXVideoCORE_SyncOperation(mfx_session_, syncp_encode, 20000);
        if (status < MFX_ERR_NONE) {
            return out_frame;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds{1});
    }
    out_frame.internal_data = buffer;
    out_frame.data = out_frame.internal_data.get();
    out_frame.size = bs.DataLength;
    out_frame.width = vppin.Info.Width;
    out_frame.height = vppin.Info.Height;
    // out_frame.frame_type = ;
    return out_frame;
}

bool IntelEncoderImpl::init_encoder() {
    auto params = gen_encode_param();
    mfxStatus status = MFXVideoENCODE_Query(mfx_session_, &params, &params);
    if (status != MFX_ERR_NONE) {
        return false;
    }
    mfxFrameAllocRequest request{};
    status = MFXVideoENCODE_QueryIOSurf(mfx_session_, &params, &request);
    if (status != MFX_ERR_NONE) {
        return false;
    }
    // 不打算Alloc
    status = MFXVideoENCODE_Init(mfx_session_, &params);
    if (status != MFX_ERR_NONE) {
        return false;
    }
    mfxVideoParam param_out;
    memset(&param_out, 0, sizeof(param_out));
    status = MFXVideoENCODE_GetVideoParam(mfx_session_, &param_out);
    if (status != MFX_ERR_NONE) {
        return false;
    }
    encode_param_ = param_out;
    return true;
}

bool IntelEncoderImpl::init_vpp() {
    mfxVideoParam params = gen_vpp_param();
    std::array<mfxFrameAllocRequest, 2> requests;
    memset(requests.data(), 0, requests.size() * sizeof(mfxFrameAllocRequest));
    // 这步其实可以不需要
    mfxStatus status = MFXVideoVPP_QueryIOSurf(mfx_session_, &params, requests.data());
    if (status != MFX_ERR_NONE) {
        return false;
    }
    render_surface_ = alloc_render_surface();
    status = MFXVideoVPP_Init(mfx_session_, &params);
    if (status != MFX_ERR_NONE) {
        return false;
    }
    vpp_param_ = params;
    return true;
}

Microsoft::WRL::ComPtr<ID3D11Texture2D> IntelEncoderImpl::alloc_render_surface() {
    Microsoft::WRL::ComPtr<ID3D11Texture2D> frame;
    D3D11_TEXTURE2D_DESC desc;
    memset(&desc, 0, sizeof(desc));
    desc.Width = 1920;
    desc.Height = 1088; // 这里填1080会出问题
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.Format = DXGI_FORMAT_NV12;
    desc.BindFlags = D3D11_BIND_RENDER_TARGET;
    desc.MiscFlags = 0;
    // desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
    HRESULT hr = device_->CreateTexture2D(&desc, nullptr, frame.GetAddressOf());
    if (FAILED(hr)) {
        // assert(false);
        return nullptr;
    }
    return frame;
}

mfxVideoParam IntelEncoderImpl::gen_encode_param() {
#define MSDK_ALIGN16(value) (((value + 15) >> 4) << 4)
#define MSDK_ALIGN32(X) (((mfxU32)((X) + 31)) & (~(mfxU32)31))
    mfxVideoParam params;
    memset(&params, 0, sizeof(params));
    params.mfx.CodecId = codec_type_ == rtc::VideoCodecType::H264 ? MFX_CODEC_AVC : MFX_CODEC_HEVC;
    params.mfx.TargetUsage = MFX_TARGETUSAGE_BEST_SPEED;
    params.mfx.TargetKbps = 3 * 1024;
    params.mfx.RateControlMethod = MFX_RATECONTROL_VBR;
    params.mfx.GopRefDist = 1;
    params.mfx.GopPicSize = static_cast<mfxU16>(1000000);
    params.mfx.NumRefFrame = 1;
    params.mfx.IdrInterval = 0; // 未填
    params.mfx.CodecProfile = static_cast<mfxU16>(
        codec_type_ == rtc::VideoCodecType::H264 ? MFX_PROFILE_AVC_MAIN : MFX_PROFILE_HEVC_MAIN);
    params.mfx.CodecLevel = 0; // 未填
    params.mfx.MaxKbps = 20 * 1024;
    params.mfx.InitialDelayInKB = 0; // 未填
    params.mfx.GopOptFlag = 0;       // 未填
    params.mfx.BufferSizeInKB = 512;
    params.mfx.NumSlice = 0; // 未填值
    params.mfx.EncodedOrder = 0;
    params.IOPattern = MFX_IOPATTERN_IN_VIDEO_MEMORY;
    if (enable_qsvff_) {
        params.mfx.LowPower = MFX_CODINGOPTION_ON;
    }
    ConvertFrameRate(30, &params.mfx.FrameInfo.FrameRateExtN, &params.mfx.FrameInfo.FrameRateExtD);

    params.mfx.FrameInfo.FourCC = MFX_FOURCC_RGB4;
    params.mfx.FrameInfo.ChromaFormat = FourCCToChroma(MFX_FOURCC_RGB4);
    params.mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
    params.mfx.FrameInfo.Shift = 0;
    params.mfx.FrameInfo.CropX = 0;
    params.mfx.FrameInfo.CropY = 0;
    params.mfx.FrameInfo.CropW = static_cast<mfxU16>(width_);
    params.mfx.FrameInfo.CropH = static_cast<mfxU16>(height_);
    params.mfx.FrameInfo.Width = MSDK_ALIGN16(static_cast<mfxU16>(width_));
    params.mfx.FrameInfo.Height = MSDK_ALIGN32(static_cast<mfxU16>(height_));

    // auto codingOption = params.AddExtBuffer<mfxExtCodingOption>();
    // codingOption->PicTimingSEI = pInParams->nPicTimingSEI;
    // codingOption->NalHrdConformance = pInParams->nNalHrdConformance;
    // codingOption->VuiNalHrdParameters = pInParams->nVuiNalHrdParameters;

    params.AsyncDepth = 1;
    return params;
#undef MSDK_ALIGN32
#undef MSDK_ALIGN16
}

mfxVideoParam IntelEncoderImpl::gen_vpp_param() {
    mfxVideoParam params;
    memset(&params, 0, sizeof(params));
#define MSDK_ALIGN16(value) (((value + 15) >> 4) << 4)
#define MSDK_ALIGN32(X) (((mfxU32)((X) + 31)) & (~(mfxU32)31))
    params.IOPattern = MFX_IOPATTERN_IN_VIDEO_MEMORY | MFX_IOPATTERN_OUT_VIDEO_MEMORY;
    // Input data
    params.vpp.In.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
    ConvertFrameRate(30, &params.mfx.FrameInfo.FrameRateExtN, &params.mfx.FrameInfo.FrameRateExtD);
    params.vpp.In.AspectRatioW = 1;
    params.vpp.In.AspectRatioH = 1;
    params.vpp.In.FourCC = MFX_FOURCC_RGB4;
    params.vpp.In.ChromaFormat = FourCCToChroma(params.vpp.In.FourCC);
    params.vpp.In.CropX = 0;
    params.vpp.In.CropY = 0;
    params.vpp.In.CropW = static_cast<mfxU16>(width_);
    params.vpp.In.CropH = static_cast<mfxU16>(height_);
    params.vpp.In.Width = MSDK_ALIGN16(static_cast<mfxU16>(width_));
    params.vpp.In.Height = MSDK_ALIGN16(static_cast<mfxU16>(height_));
    params.vpp.In.Shift = 0;
    // Output data
    memcpy(&params.vpp.Out, &params.vpp.In, sizeof(params.vpp.In));

    // chromasiting
    // auto colorconversion = m_mfxVppParams.AddExtBuffer<mfxExtColorConversion>();
    // colorconversion->ChromaSiting = MFX_CHROMA_SITING_VERTICAL_CENTER |
    // MFX_CHROMA_SITING_HORIZONTAL_CENTER; colorconversion->Header.BufferId =
    // MFX_EXTBUFF_VPP_COLOR_CONVERSION; colorconversion->Header.BufferSz =
    // sizeof(mfxExtColorConversion);

    // BT709
    // auto videosignalinfo = m_mfxVppParams.AddExtBuffer<mfxExtVPPVideoSignalInfo>(); //1221
    // videosignalinfo->In.NominalRange = 1; //specify YUV nominal range for input surface: 0 -
    // unknown; 1 - [0...255]; 2 - [16...235] videosignalinfo->Out.NominalRange = 2; //specify YUV
    // nominal range for output surface: 0 - unknown; 1 - [0...255]; 2 - [16...235]
    // videosignalinfo->In.TransferMatrix = 2; //specify YUV<->RGB transfer matrix for input
    // surface: 0 - unknown; 1 - BT709; 2 - BT601 videosignalinfo->Out.TransferMatrix = 1; //specify
    // YUV<->RGB transfer matrix for output surface: 0 - unknown; 1 - BT709; 2 - BT601

#undef MSDK_ALIGN32
#undef MSDK_ALIGN16
    return params;
}

} // namespace lt