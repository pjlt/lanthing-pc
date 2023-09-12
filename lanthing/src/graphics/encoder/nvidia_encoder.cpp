#include "nvidia_encoder.h"

#include <d3d11_1.h>
#include <wrl/client.h>

#include <cassert>
#include <memory>
#include <sstream>
#include <string>

#include <nvcodec/nvEncodeAPI.h>

#include <g3log/g3log.hpp>

#include <ltlib/load_library.h>

namespace {

DXGI_FORMAT toDxgiFormat(NV_ENC_BUFFER_FORMAT format) {
    switch (format) {
    case NV_ENC_BUFFER_FORMAT_NV12:
        return DXGI_FORMAT_NV12;
    case NV_ENC_BUFFER_FORMAT_ARGB:
        return DXGI_FORMAT_B8G8R8A8_UNORM;
    default:
        return DXGI_FORMAT_UNKNOWN;
    }
}

class NvEncParamsHelper {
public:
    NvEncParamsHelper(const lt::VideoEncodeParamsHelper& params)
        : params_{params} {}

    int fps() const { return params_.fps(); }

    uint32_t bitrate() const { return params_.bitrate(); }
    uint32_t maxbitrate() const { return static_cast<uint32_t>(bitrate() * 1.05f); }
    NV_ENC_QP qmin() const { return {params_.qmin()[0], params_.qmin()[1], params_.qmin()[2]}; }
    NV_ENC_QP qmax() const { return {params_.qmax()[0], params_.qmax()[1], params_.qmax()[2]}; }
    std::optional<int> vbvbufsize() const { return params_.vbvbufsize(); }
    std::optional<int> vbvinit() const { return params_.vbvinit(); }
    int gop() const { return params_.gop(); }
    NV_ENC_PARAMS_RC_MODE rc() const;
    GUID preset() const;
    GUID codec() const;
    GUID profile() const;

private:
    const lt::VideoEncodeParamsHelper params_;
};

NV_ENC_PARAMS_RC_MODE NvEncParamsHelper::rc() const {
    switch (params_.rc()) {
    case lt::VideoEncodeParamsHelper::RcMode::CBR:
        return NV_ENC_PARAMS_RC_CBR;
    case lt::VideoEncodeParamsHelper::RcMode::VBR:
        return NV_ENC_PARAMS_RC_VBR;
    default:
        assert(false);
        return NV_ENC_PARAMS_RC_CBR;
    }
}

GUID NvEncParamsHelper::preset() const {
    switch (params_.preset()) {
    case lt::VideoEncodeParamsHelper::Preset::Balanced:
        return NV_ENC_PRESET_LOW_LATENCY_DEFAULT_GUID;
    case lt::VideoEncodeParamsHelper::Preset::Speed:
        return NV_ENC_PRESET_LOW_LATENCY_HP_GUID;
    case lt::VideoEncodeParamsHelper::Preset::Quality:
        return NV_ENC_PRESET_LOW_LATENCY_HQ_GUID;
    default:
        assert(false);
        return NV_ENC_PRESET_LOW_LATENCY_DEFAULT_GUID;
    }
}

GUID NvEncParamsHelper::codec() const {
    switch (params_.codec()) {
    case lt::VideoCodecType::H264:
        return NV_ENC_CODEC_H264_GUID;
    case lt::VideoCodecType::H265:
        return NV_ENC_CODEC_HEVC_GUID;
    default:
        assert(false);
        return NV_ENC_CODEC_H264_GUID;
    }
}

GUID NvEncParamsHelper::profile() const {
    switch (params_.profile()) {
    case lt::VideoEncodeParamsHelper::Profile::AvcMain:
        return NV_ENC_H264_PROFILE_MAIN_GUID;
    case lt::VideoEncodeParamsHelper::Profile::HevcMain:
        return NV_ENC_H264_PROFILE_HIGH_GUID;
    default:
        assert(false);
        return NV_ENC_H264_PROFILE_MAIN_GUID;
    }
}

} // namespace

namespace lt {

class NvD3d11EncoderImpl {
public:
    NvD3d11EncoderImpl(ID3D11Device* d3d11_dev);
    ~NvD3d11EncoderImpl();
    bool init(const VideoEncodeParamsHelper& params);
    void reconfigure(const VideoEncoder::ReconfigureParams& params);
    VideoEncoder::EncodedFrame encodeOneFrame(void* input_frame, bool request_iframe);

private:
    bool loadNvApi();
    NV_ENC_INITIALIZE_PARAMS generateEncodeParams(const NvEncParamsHelper& helper,
                                                  NV_ENC_CONFIG& config);
    bool initBuffers();
    std::optional<NV_ENC_MAP_INPUT_RESOURCE> initInputFrame(void* frame);
    bool uninitInputFrame(NV_ENC_MAP_INPUT_RESOURCE& resource);
    void releaseResources();

private:
    Microsoft::WRL::ComPtr<ID3D11Device> d3d11_dev_;
    uint32_t width_;
    uint32_t height_;
    lt::VideoCodecType codec_type_;
    std::unique_ptr<ltlib::DynamicLibrary> nvapi_;
    NV_ENCODE_API_FUNCTION_LIST nvfuncs_;
    void* nvencoder_;
    NV_ENC_BUFFER_FORMAT buffer_format_ = NV_ENC_BUFFER_FORMAT_ARGB;
    NV_ENC_INITIALIZE_PARAMS init_params_;
    NV_ENC_CONFIG encode_config_;
    void* bitstream_output_buffer_ = nullptr;
    NV_ENC_REGISTER_RESOURCE register_res_ = {NV_ENC_REGISTER_RESOURCE_VER};
};

NvD3d11EncoderImpl::NvD3d11EncoderImpl(ID3D11Device* dev)
    : d3d11_dev_{dev} {}

NvD3d11EncoderImpl::~NvD3d11EncoderImpl() {
    releaseResources();
}

bool NvD3d11EncoderImpl::init(const VideoEncodeParamsHelper& params) {
    NvEncParamsHelper params_helper{params};
    width_ = params.width();
    height_ = params.height();
    codec_type_ = params.codec();
    if (codec_type_ == lt::VideoCodecType::H264 &&
        (buffer_format_ == NV_ENC_BUFFER_FORMAT_YUV420_10BIT ||
         buffer_format_ == NV_ENC_BUFFER_FORMAT_YUV444_10BIT)) {
        LOG(WARNING) << "Unsupported buffer format " << buffer_format_ << " when using h264";
        return false;
    }
    if (!loadNvApi()) {
        return false;
    }
    if (nvfuncs_.nvEncOpenEncodeSessionEx == nullptr) {
        LOG(WARNING) << "nvEncodeAPI not found";
        return false;
    }
    NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS nvparams = {NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS_VER};
    nvparams.device = d3d11_dev_.Get();
    nvparams.deviceType = NV_ENC_DEVICE_TYPE_DIRECTX;
    nvparams.apiVersion = NVENCAPI_VERSION;
    void* encoder = nullptr;
    NVENCSTATUS status = nvfuncs_.nvEncOpenEncodeSessionEx(&nvparams, &encoder);
    if (status != NV_ENC_SUCCESS) {
        LOG(WARNING) << "nvEncOpenEncodeSessionEx failed with " << status;
        return false;
    }
    nvencoder_ = encoder;

    init_params_ = generateEncodeParams(params_helper, encode_config_);

    status = nvfuncs_.nvEncInitializeEncoder(nvencoder_, &init_params_);
    if (status != NV_ENC_SUCCESS) {
        LOG(WARNING) << "nvEncInitializeEncoder failed with " << status;
        return false;
    }
    if (!initBuffers()) {
        return false;
    }
    return true;
}

void NvD3d11EncoderImpl::releaseResources() {
    if (nvencoder_ == nullptr) {
        return;
    }
    if (bitstream_output_buffer_ != nullptr) {
        nvfuncs_.nvEncDestroyBitstreamBuffer(nvencoder_, bitstream_output_buffer_);
    }
    nvfuncs_.nvEncDestroyEncoder(nvencoder_);
    nvencoder_ = nullptr;
}

void NvD3d11EncoderImpl::reconfigure(const VideoEncoder::ReconfigureParams& params) {
    NV_ENC_RECONFIGURE_PARAMS reconfigure_params{NV_ENC_RECONFIGURE_PARAMS_VER};
    bool changed = false;
    if (params.bitrate_bps.has_value()) {
        encode_config_.rcParams.averageBitRate = params.bitrate_bps.value();
        encode_config_.rcParams.maxBitRate =
            static_cast<uint32_t>(params.bitrate_bps.value() * 1.05f);
        changed = true;
    }
    if (params.fps.has_value()) {
        init_params_.frameRateNum = params.fps.value();
        changed = true;
    }
    if (!changed) {
        return;
    }
    reconfigure_params.reInitEncodeParams = init_params_;
    NVENCSTATUS status = nvfuncs_.nvEncReconfigureEncoder(nvencoder_, &reconfigure_params);
    if (status != NV_ENC_SUCCESS) {
        LOG(WARNING) << "nvEncReconfigureEncoder failed with " << status;
    }
}

VideoEncoder::EncodedFrame NvD3d11EncoderImpl::encodeOneFrame(void* input_frame,
                                                              bool request_iframe) {
    VideoEncoder::EncodedFrame out_frame{};
    auto mapped_resource = initInputFrame(input_frame);
    if (!mapped_resource.has_value()) {
        return out_frame; //??
    }

    NV_ENC_PIC_PARAMS params{};
    params.version = NV_ENC_PIC_PARAMS_VER;
    params.encodePicFlags =
        request_iframe ? (NV_ENC_PIC_FLAG_FORCEIDR | NV_ENC_PIC_FLAG_OUTPUT_SPSPPS) : 0;
    params.pictureStruct = NV_ENC_PIC_STRUCT_FRAME;
    params.inputBuffer = mapped_resource->mappedResource;
    params.bufferFmt = buffer_format_;
    params.inputWidth = width_;
    params.inputHeight = height_;
    params.outputBitstream = bitstream_output_buffer_;
    NVENCSTATUS status = nvfuncs_.nvEncEncodePicture(nvencoder_, &params);
    if (status != NV_ENC_SUCCESS) {
        // include NV_ENC_ERR_NEED_MORE_INPUT
        LOG(WARNING) << "nvEncEncodePicture failed with " << status;
    }
    NV_ENC_LOCK_BITSTREAM lbs = {NV_ENC_LOCK_BITSTREAM_VER};
    lbs.outputBitstream = bitstream_output_buffer_;
    lbs.doNotWait = false;
    status = nvfuncs_.nvEncLockBitstream(nvencoder_, &lbs);
    if (status != NV_ENC_SUCCESS) {
        LOG(WARNING) << "nvEncLockBitstream failed with " << status;
        return out_frame; // ??
    }
    out_frame.size = lbs.bitstreamSizeInBytes;
    out_frame.internal_data = std::shared_ptr<uint8_t>(new uint8_t[out_frame.size]);
    memcpy(out_frame.internal_data.get(), lbs.bitstreamBufferPtr, out_frame.size);
    out_frame.data = out_frame.internal_data.get();
    status = nvfuncs_.nvEncUnlockBitstream(nvencoder_, lbs.outputBitstream);
    if (status != NV_ENC_SUCCESS) {
        LOG(WARNING) << "nvEncUnlockBitstream failed with " << status;
        return out_frame; // ??
    }

    if (!uninitInputFrame(mapped_resource.value())) {
        return out_frame; //??
    }

    NV_ENC_STAT encode_stats{};
    encode_stats.outputBitStream = params.outputBitstream;
    encode_stats.version = NV_ENC_STAT_VER;
    nvfuncs_.nvEncGetEncodeStats(nvencoder_, &encode_stats);
    out_frame.is_keyframe =
        encode_stats.picType == NV_ENC_PIC_TYPE_I || encode_stats.picType == NV_ENC_PIC_TYPE_IDR;
    return out_frame;
}

bool NvD3d11EncoderImpl::loadNvApi() {
#if defined(LT_WINDOWS)
    std::string lib_name = "nvEncodeAPI64.dll";
#else
    std::string lib_name = "libnvidia-encode.so.1";
#endif
    nvapi_ = ltlib::DynamicLibrary::load(lib_name);
    if (nvapi_ == nullptr) {
        return false;
    }

    using NvEncodeAPIGetMaxSupportedVersionType = NVENCSTATUS(NVENCAPI*)(uint32_t*);
    auto NvEncodeAPIGetMaxSupportedVersionFunc =
        reinterpret_cast<NvEncodeAPIGetMaxSupportedVersionType>(
            nvapi_->get_func("NvEncodeAPIGetMaxSupportedVersion"));
    if (NvEncodeAPIGetMaxSupportedVersionFunc == nullptr) {
        LOG(WARNING) << "Load 'NvEncodeAPIGetMaxSupportedVersion' from '" << lib_name << "' failed";
        return false;
    }

    uint32_t machine_version = 0;
    uint32_t sdk_version = (NVENCAPI_MAJOR_VERSION << 4) | NVENCAPI_MINOR_VERSION;
    NVENCSTATUS status = NvEncodeAPIGetMaxSupportedVersionFunc(&machine_version);
    if (status != NV_ENC_SUCCESS) {
        LOG(WARNING) << "NvEncodeAPIGetMaxSupportedVersion failed with " << status;
        return false;
    }
    if (machine_version < sdk_version) {
        LOG(WARNING) << "Nvidia GPU driver too old!";
        return false;
    }

    using NvEncodeAPICreateInstanceType = NVENCSTATUS(NVENCAPI*)(NV_ENCODE_API_FUNCTION_LIST*);
    auto NvEncodeAPICreateInstanceFunc = reinterpret_cast<NvEncodeAPICreateInstanceType>(
        nvapi_->get_func("NvEncodeAPICreateInstance"));
    if (NvEncodeAPICreateInstanceFunc == nullptr) {
        LOG(WARNING) << "Load 'NvEncodeAPICreateInstance' from '" << lib_name << "' failed";
        return false;
    }

    nvfuncs_ = {NV_ENCODE_API_FUNCTION_LIST_VER};
    status = NvEncodeAPICreateInstanceFunc(&nvfuncs_);
    if (status != NV_ENC_SUCCESS) {
        LOG(WARNING) << "NvEncodeAPICreateInstance failed with " << status;
        return false;
    }
    return true;
}

NV_ENC_INITIALIZE_PARAMS
NvD3d11EncoderImpl::generateEncodeParams(const NvEncParamsHelper& helper,
                                         NV_ENC_CONFIG& encode_config) {
    NV_ENC_INITIALIZE_PARAMS params{};
    ::memset(&encode_config, 0, sizeof(NV_ENC_CONFIG));
    encode_config.version = NV_ENC_CONFIG_VER;
    params.encodeConfig = &encode_config;
    params.version = NV_ENC_INITIALIZE_PARAMS_VER;
    params.encodeGUID = helper.codec();
    params.presetGUID = helper.preset();
    params.encodeWidth = width_;
    params.encodeHeight = height_;
    params.darWidth = width_;
    params.darHeight = height_;
    params.maxEncodeWidth = width_;
    params.maxEncodeHeight = height_;
    params.frameRateNum = helper.fps();
    params.frameRateDen = 1;
    params.enablePTD = 1;
    params.reportSliceOffsets = 0;
    params.enableSubFrameWrite = 0;
    params.enableEncodeAsync = false;

    NV_ENC_PRESET_CONFIG preset_config = {NV_ENC_PRESET_CONFIG_VER, {NV_ENC_CONFIG_VER}};
    nvfuncs_.nvEncGetEncodePresetConfig(nvencoder_, params.encodeGUID, params.presetGUID,
                                        &preset_config);
    ::memcpy(params.encodeConfig, &preset_config.presetCfg, sizeof(NV_ENC_CONFIG)); //???
    params.encodeConfig->frameIntervalP = 1;
    params.encodeConfig->gopLength = NVENC_INFINITE_GOPLENGTH;
    params.encodeConfig->rcParams.rateControlMode = helper.rc();
    params.encodeConfig->rcParams.averageBitRate = helper.bitrate();
    params.encodeConfig->rcParams.maxBitRate = helper.maxbitrate();
    params.encodeConfig->rcParams.minQP = helper.qmin();
    params.encodeConfig->rcParams.enableMinQP = true;
    params.encodeConfig->rcParams.maxQP = helper.qmax();
    params.encodeConfig->rcParams.enableMaxQP = true;
    if (helper.vbvbufsize().has_value()) {
        params.encodeConfig->rcParams.vbvBufferSize = helper.vbvbufsize().value();
    }
    if (helper.vbvinit().has_value()) {
        params.encodeConfig->rcParams.vbvInitialDelay = helper.vbvinit().value();
    }

    if (params.presetGUID != NV_ENC_PRESET_LOSSLESS_DEFAULT_GUID &&
        params.presetGUID != NV_ENC_PRESET_LOSSLESS_HP_GUID) {
        params.encodeConfig->rcParams.constQP = {28, 31, 25};
    }

    if (params.encodeGUID == NV_ENC_CODEC_H264_GUID) {
        if (buffer_format_ == NV_ENC_BUFFER_FORMAT_YUV444 ||
            buffer_format_ == NV_ENC_BUFFER_FORMAT_YUV444_10BIT) {
            params.encodeConfig->encodeCodecConfig.h264Config.chromaFormatIDC = 3;
        }
        params.encodeConfig->encodeCodecConfig.h264Config.idrPeriod =
            params.encodeConfig->gopLength;
        params.encodeConfig->encodeCodecConfig.h264Config.maxNumRefFrames = 0;
        params.encodeConfig->encodeCodecConfig.h264Config.sliceMode = 3;
        params.encodeConfig->encodeCodecConfig.h264Config.sliceModeData = 1;
    }
    else if (params.encodeGUID == NV_ENC_CODEC_HEVC_GUID) {
        params.encodeConfig->encodeCodecConfig.hevcConfig.pixelBitDepthMinus8 =
            (buffer_format_ == NV_ENC_BUFFER_FORMAT_YUV420_10BIT ||
             buffer_format_ == NV_ENC_BUFFER_FORMAT_YUV444_10BIT)
                ? 2
                : 0;
        if (buffer_format_ == NV_ENC_BUFFER_FORMAT_YUV444 ||
            buffer_format_ == NV_ENC_BUFFER_FORMAT_YUV444_10BIT) {
            params.encodeConfig->encodeCodecConfig.hevcConfig.chromaFormatIDC = 3;
        }
        params.encodeConfig->encodeCodecConfig.hevcConfig.idrPeriod =
            params.encodeConfig->gopLength;
        params.encodeConfig->encodeCodecConfig.hevcConfig.maxNumRefFramesInDPB = 0;
        params.encodeConfig->encodeCodecConfig.hevcConfig.sliceMode = 3;
        params.encodeConfig->encodeCodecConfig.hevcConfig.sliceModeData = 1;
    }
    return params;
}

bool NvD3d11EncoderImpl::initBuffers() {
    NV_ENC_CREATE_BITSTREAM_BUFFER bits_params = {NV_ENC_CREATE_BITSTREAM_BUFFER_VER};
    NVENCSTATUS status = nvfuncs_.nvEncCreateBitstreamBuffer(nvencoder_, &bits_params);
    if (status != NV_ENC_SUCCESS) {
        LOG(WARNING) << "nvEncCreateBitstreamBuffer failed with " << status;
    }
    bitstream_output_buffer_ = bits_params.bitstreamBuffer;
    return true;
}

std::optional<NV_ENC_MAP_INPUT_RESOURCE> NvD3d11EncoderImpl::initInputFrame(void* frame) {
    if (register_res_.resourceToRegister && register_res_.resourceToRegister != frame) {
        NVENCSTATUS status =
            nvfuncs_.nvEncUnregisterResource(nvencoder_, register_res_.registeredResource);
        if (status != NV_ENC_SUCCESS) {
            LOG(WARNING) << "nvEncUnregisterResource failed with " << status;
            return {};
        }
    }
    if (register_res_.resourceToRegister == nullptr) {
        register_res_.resourceToRegister = frame;
        NVENCSTATUS status = nvfuncs_.nvEncRegisterResource(nvencoder_, &register_res_);
        if (status != NV_ENC_SUCCESS) {
            LOG(WARNING) << "nvEncRegisterResource failed with " << status;
            return {};
        }
    }
    NV_ENC_MAP_INPUT_RESOURCE mapped_resource = {NV_ENC_MAP_INPUT_RESOURCE_VER};
    mapped_resource.registeredResource = register_res_.registeredResource;
    NVENCSTATUS status = nvfuncs_.nvEncMapInputResource(nvencoder_, &mapped_resource);
    if (status != NV_ENC_SUCCESS) {
        LOG(WARNING) << "nvEncMapInputResource failed with " << status;
        return {};
    }
    return mapped_resource;
}

bool NvD3d11EncoderImpl::uninitInputFrame(NV_ENC_MAP_INPUT_RESOURCE& resource) {
    NVENCSTATUS status = nvfuncs_.nvEncUnmapInputResource(nvencoder_, &resource);
    if (status != NV_ENC_SUCCESS) {
        LOG(WARNING) << "nvEncUnmapInputResource failed with " << status;
        return false;
    }
    if (register_res_.registeredResource) {
        status = nvfuncs_.nvEncUnregisterResource(nvencoder_, register_res_.registeredResource);
        if (status != NV_ENC_SUCCESS) {
            LOG(WARNING) << "nvEncUnregisterResource failed with " << status;
            return false;
        }
        register_res_.resourceToRegister = nullptr;
    }
    return true;
}

NvD3d11Encoder::NvD3d11Encoder(void* d3d11_dev, void* d3d11_ctx)
    : VideoEncoder{d3d11_dev, d3d11_ctx}
    , impl_{std::make_shared<NvD3d11EncoderImpl>(reinterpret_cast<ID3D11Device*>(d3d11_dev))} {}

NvD3d11Encoder::~NvD3d11Encoder() {}

bool NvD3d11Encoder::init(const VideoEncodeParamsHelper& params) {
    return impl_->init(params);
}

void NvD3d11Encoder::reconfigure(const ReconfigureParams& params) {
    impl_->reconfigure(params);
}

VideoEncoder::EncodedFrame NvD3d11Encoder::encodeFrame(void* input_frame) {
    return impl_->encodeOneFrame(input_frame, needKeyframe());
}

} // namespace lt