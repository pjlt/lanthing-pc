#include <d3d11_1.h>
#include <dxgi1_3.h>
#include <wrl/client.h>

#include <g3log/g3log.hpp>

#include <ltlib/strings.h>
#include <ltlib/times.h>

#include "amd_encoder.h"
#include "intel_encoder.h"
#include "nvidia_encoder.h"
#include "params_helper.h"
#include "video_encoder.h"

using Microsoft::WRL::ComPtr;

namespace {

bool is_black_frame(const lt::VideoEncoder::EncodedFrame& encoded_frame) {
    // if (!encoded_frame.is_keyframe && encoded_frame.size < 1000) {
    //     return true;
    // }
    // if (encoded_frame.is_keyframe && encoded_frame.size < 2000) {
    //     return true;
    // }
    (void)encoded_frame;
    return false;
}

std::string backend_to_string(lt::VideoEncoder::Backend backend) {
    switch (backend) {
    case lt::VideoEncoder::Backend::Unknown:
        return "Unknown";
    case lt::VideoEncoder::Backend::NvEnc:
        return "NvEnc";
    case lt::VideoEncoder::Backend::IntelMediaSDK:
        return "IntelMediaSDK";
    case lt::VideoEncoder::Backend::Amf:
        return "Amf";
    default:
        return "Unknown";
    }
}

auto create_d3d11()
    -> std::tuple<ComPtr<ID3D11Device>, ComPtr<ID3D11DeviceContext>, uint32_t, int64_t> {
    // TODO: 遍历每个GPU
    uint32_t vendor_id = 0;
    int64_t luid = 0;
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    ComPtr<IDXGIFactory2> dxgi_factory;
    auto hr = CreateDXGIFactory2(0, __uuidof(IDXGIFactory2), (void**)dxgi_factory.GetAddressOf());
    if (FAILED(hr)) {
        LOGF(WARNING, "Failed to create dxgi factory2, hr:0x%08x", hr);
        return {nullptr, nullptr, vendor_id, luid};
    }
    ComPtr<IDXGIAdapter1> adapter;
    DXGI_ADAPTER_DESC1 adapter_desc{};
    hr = dxgi_factory->EnumAdapters1(0, &adapter);
    if (hr == DXGI_ERROR_NOT_FOUND) {
        return {nullptr, nullptr, vendor_id, luid};
    }
    DXGI_ADAPTER_DESC desc;
    hr = adapter->GetDesc(&desc);
    if (hr != S_OK) {
        LOGF(WARNING, "Failed to GetDesc, err:%0x8lx", hr);
    }
    else {
        vendor_id = desc.VendorId;
        luid = ((uint64_t)desc.AdapterLuid.HighPart << 32) + desc.AdapterLuid.LowPart;
    }
    UINT flag = 0;
#ifdef _DEBUG
    flag |= D3D11_CREATE_DEVICE_DEBUG;
#endif
    hr = D3D11CreateDevice(adapter.Get(), D3D_DRIVER_TYPE_UNKNOWN, nullptr, flag, nullptr, 0,
                           D3D11_SDK_VERSION, device.GetAddressOf(), nullptr,
                           context.GetAddressOf());
    if (FAILED(hr)) {
        LOGF(WARNING, "fail to create d3d11 device, err:%08lx", hr);
        return {nullptr, nullptr, vendor_id, luid};
    }
    LOGF(INFO, "D3D11Device(index:0, %x:%x) created", desc.VendorId, desc.DeviceId);
    return {device, context, vendor_id, luid};
}

auto create_d3d11_with_luid(int64_t luid)
    -> std::tuple<ComPtr<ID3D11Device>, ComPtr<ID3D11DeviceContext>, uint32_t, int64_t> {
    uint32_t vendor_id = 0;
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    ComPtr<IDXGIFactory2> dxgi_factory;
    auto hr = CreateDXGIFactory2(0, __uuidof(IDXGIFactory2), (void**)dxgi_factory.GetAddressOf());
    if (FAILED(hr)) {
        LOGF(WARNING, "Failed to create dxgi factory2, hr:0x%08x", hr);
        return {nullptr, nullptr, vendor_id, luid};
    }
    ComPtr<IDXGIAdapter1> adapter;
    DXGI_ADAPTER_DESC desc;
    int32_t index = -1;
    while (true) {
        index += 1;
        hr = dxgi_factory->EnumAdapters1(static_cast<UINT>(index), adapter.GetAddressOf());
        if (FAILED(hr)) {
            return {nullptr, nullptr, vendor_id, luid};
        }
        hr = adapter->GetDesc(&desc);
        if (hr != S_OK) {
            LOGF(WARNING, "Failed to GetDesc, err:%0x8lx", hr);
            continue;
        }
        if (desc.AdapterLuid.LowPart == luid) {
            break;
        }
    }
    UINT flag = 0;
#ifdef _DEBUG
    flag |= D3D11_CREATE_DEVICE_DEBUG;
#endif
    hr = D3D11CreateDevice(adapter.Get(), D3D_DRIVER_TYPE_UNKNOWN, nullptr, flag, nullptr, 0,
                           D3D11_SDK_VERSION, device.GetAddressOf(), nullptr,
                           context.GetAddressOf());
    if (FAILED(hr)) {
        LOGF(WARNING, "fail to create d3d11 device, err:%08lx", hr);
        return {nullptr, nullptr, vendor_id, luid};
    }
    vendor_id = desc.VendorId;
    LOGF(INFO, "D3D11Device(index:0, %x:%x, %x) created", desc.VendorId, desc.DeviceId,
         desc.AdapterLuid.LowPart);
    return {device, context, vendor_id, luid};
}

auto create_d3d11(std::optional<int64_t> luid)
    -> std::tuple<ComPtr<ID3D11Device>, ComPtr<ID3D11DeviceContext>, uint32_t, int64_t> {

    if (luid.has_value()) {
        return create_d3d11_with_luid(luid.value());
    }
    else {
        return create_d3d11();
    }
}

std::unique_ptr<lt::VideoEncoder> do_create_encoder(const lt::VideoEncoder::InitParams& params,
                                                    void* d3d11_dev, void* d3d11_ctx) {
    using namespace lt;
    VideoEncodeParamsHelper params_helper{
        params.codec_type, params.width, params.height, 60, params.bitrate_bps / 1024, false};
    switch (params.backend) {
    case VideoEncoder::Backend::NvEnc:
    {
        auto encoder = std::make_unique<NvD3d11Encoder>(d3d11_dev, d3d11_ctx);
        if (encoder->init(params_helper)) {
            LOG(INFO) << "NvidiaEncoder created";
            return encoder;
        }
        else {
            LOGF(INFO, "Create NvidiaEncoder(w:%u,h:%u,c:%d) failed", params.width, params.height,
                 params.codec_type);
            return nullptr;
        }
    }
    case VideoEncoder::Backend::IntelMediaSDK:
    {
        auto encoder = std::make_unique<IntelEncoder>(d3d11_dev, d3d11_ctx, params.luid);
        if (encoder->init(params_helper)) {
            LOG(INFO) << "IntelEncoder created";
            return encoder;
        }
        else {
            LOGF(INFO, "Create IntelEncoder(w:%u,h:%u,c:%d) failed", params.width, params.height,
                 params.codec_type);
            return nullptr;
        }
    }
    case VideoEncoder::Backend::Amf:
    {
        auto encoder = std::make_unique<AmdEncoder>(d3d11_dev, d3d11_ctx);
        if (encoder->init(params_helper)) {
            LOG(INFO) << "AmdEncoder created";
            return encoder;
        }
        else {
            LOGF(INFO, "Create AmdEncoder(w:%u,h:%u,c:%d) failed", params.width, params.height,
                 params.codec_type);
            return nullptr;
        }
    }
    default:
        LOG(WARNING) << "Unsupport encoder backend " << backend_to_string(params.backend);
        return nullptr;
    }
}

std::vector<lt::VideoEncoder::Ability>
do_check_encode_abilities(ComPtr<ID3D11Device> device, ComPtr<ID3D11DeviceContext> context,
                          uint32_t vendor_id, int64_t luid, uint32_t width, uint32_t height) {
    using namespace lt;
    constexpr uint32_t kAMDVendorID = 0x1002;
    constexpr uint32_t kIntelVendorID = 0x8086;
    constexpr uint32_t kNvidiaVendorID = 0x10DE;
    auto check_with_backend_and_codec = [luid, width, height, dev = device.Get(),
                                         ctx = context.Get()](VideoEncoder::Backend backend,
                                                              lt::VideoCodecType codec) -> bool {
        VideoEncoder::InitParams params;
        params.backend = backend;
        params.codec_type = codec;
        params.width = width;
        params.height = height;
        params.bitrate_bps = 10'000;
        params.luid = luid;
        auto encoder = do_create_encoder(params, dev, ctx);
        return encoder != nullptr;
    };

    auto check_with_order =
        [width, height, dev = device.Get(), ctx = context.Get(), check_with_backend_and_codec](
            std::vector<VideoEncoder::Backend> backend_order,
            std::vector<lt::VideoCodecType> codec_order) -> std::vector<VideoEncoder::Ability> {
        std::vector<VideoEncoder::Ability> abilities;
        for (auto backend : backend_order) {
            for (auto codec_type : codec_order) {
                if (check_with_backend_and_codec(backend, codec_type)) {
                    abilities.push_back({backend, codec_type});
                }
            }
            // 某一backend下，如果检测成功，则立马返回，因为一块显卡不可能支持NvEnc的同时，又支持IntelMediaSDK
            if (!abilities.empty()) {
                return abilities;
            }
        }
        return abilities;
    };

    std::vector<VideoEncoder::Ability> abilities;
    switch (vendor_id) {
    case kIntelVendorID:
        if (check_with_backend_and_codec(VideoEncoder::Backend::IntelMediaSDK,
                                         lt::VideoCodecType::H265)) {
            abilities.push_back({VideoEncoder::Backend::IntelMediaSDK, lt::VideoCodecType::H265});
        }
        if (check_with_backend_and_codec(VideoEncoder::Backend::IntelMediaSDK,
                                         lt::VideoCodecType::H264)) {
            abilities.push_back({VideoEncoder::Backend::IntelMediaSDK, lt::VideoCodecType::H264});
        }
        break;
    case kNvidiaVendorID:
        if (check_with_backend_and_codec(VideoEncoder::Backend::NvEnc, lt::VideoCodecType::H265)) {
            abilities.push_back({VideoEncoder::Backend::NvEnc, lt::VideoCodecType::H265});
        }
        if (check_with_backend_and_codec(VideoEncoder::Backend::NvEnc, lt::VideoCodecType::H264)) {
            abilities.push_back({VideoEncoder::Backend::NvEnc, lt::VideoCodecType::H264});
        }
        break;
    case kAMDVendorID:
        if (check_with_backend_and_codec(VideoEncoder::Backend::Amf, lt::VideoCodecType::H265)) {
            abilities.push_back({VideoEncoder::Backend::Amf, lt::VideoCodecType::H265});
        }
        if (check_with_backend_and_codec(VideoEncoder::Backend::Amf, lt::VideoCodecType::H264)) {
            abilities.push_back({VideoEncoder::Backend::Amf, lt::VideoCodecType::H264});
        }
        break;
    default:
        abilities =
            check_with_order({VideoEncoder::Backend::NvEnc, VideoEncoder::Backend::IntelMediaSDK,
                              VideoEncoder::Backend::Amf},
                             {lt::VideoCodecType::H265, lt::VideoCodecType::H264});
        break;
    }
    return abilities;
}

} // namespace

namespace lt {

std::unique_ptr<VideoEncoder> VideoEncoder::create(const InitParams& params) {
    if (!params.validate()) {
        LOG(WARNING) << "Create VideoEncoder failed: invalid parameters";
        return nullptr;
    }
    auto [device, context, vendor_id, luid] = create_d3d11(params.luid);
    if (device == nullptr || context == nullptr) {
        return nullptr;
    }
    return do_create_encoder(params, device.Get(), context.Get());
}

VideoEncoder::VideoEncoder(void* d3d11_dev, void* d3d11_ctx)
    : d3d11_dev_{d3d11_dev}
    , d3d11_ctx_{d3d11_ctx} {
    auto dev = reinterpret_cast<ID3D11Device*>(d3d11_dev_);
    dev->AddRef();
    auto ctx = reinterpret_cast<ID3D11DeviceContext*>(d3d11_ctx_);
    ctx->AddRef();
}

bool VideoEncoder::needKeyframe() {
    return request_keyframe_.exchange(false);
}

VideoEncoder::~VideoEncoder() {
    if (d3d11_dev_) {
        auto dev = reinterpret_cast<ID3D11Device*>(d3d11_dev_);
        dev->Release();
    }
    if (d3d11_ctx_) {
        auto ctx = reinterpret_cast<ID3D11DeviceContext*>(d3d11_ctx_);
        ctx->Release();
    }
}

void VideoEncoder::requestKeyframe() {
    request_keyframe_ = true;
}

VideoEncoder::EncodedFrame
VideoEncoder::encode(std::shared_ptr<ltproto::peer2peer::CaptureVideoFrame> input_frame) {
    if (input_frame->underlying_type() !=
        ltproto::peer2peer::CaptureVideoFrame_UnderlyingType_DxgiSharedHandle) {
        LOG(FATAL) << "Only support DxgiSharedHandle!";
        return {};
    }
    std::wstring name = ltlib::utf8_to_utf16(input_frame->name());
    Microsoft::WRL::ComPtr<ID3D11Device1> d3d11_1_dev;
    Microsoft::WRL::ComPtr<ID3D11Device> d3d11_dev = reinterpret_cast<ID3D11Device*>(d3d11_dev_);
    d3d11_dev.As(&d3d11_1_dev);
    Microsoft::WRL::ComPtr<ID3D11Resource> resource;
    auto hr = d3d11_1_dev->OpenSharedResourceByName(name.c_str(), DXGI_SHARED_RESOURCE_READ,
                                                    __uuidof(ID3D11Resource),
                                                    (void**)resource.GetAddressOf());
    if (FAILED(hr)) {
        LOGF(WARNING, "Failed to open shared resource, hr:0x%08x", hr);
        return {};
    }
    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
    resource.As(&texture);
    Microsoft::WRL::ComPtr<IDXGIKeyedMutex> mutex;
    resource.As(&mutex);
    hr = mutex->AcquireSync(1, 0);
    if (hr != S_OK) {
        LOGF(WARNING, "Failed to get dxgi mutex, hr:0x%08x", hr);
        mutex->ReleaseSync(0);
        return {};
    }

    const int64_t start_encode = ltlib::steady_now_us();
    auto encoded_frame = this->encodeFrame(texture.Get());
    const int64_t end_encode = ltlib::steady_now_us();

    encoded_frame.is_black_frame = is_black_frame(encoded_frame);
    encoded_frame.start_encode_timestamp_us = start_encode;
    encoded_frame.end_encode_timestamp_us = end_encode;
    encoded_frame.ltframe_id = frame_id_++;
    encoded_frame.capture_timestamp_us = input_frame->capture_timestamp_us();
    encoded_frame.width = input_frame->width();
    encoded_frame.height = input_frame->height();
    mutex->ReleaseSync(0);
    if (!first_frame_) {
        first_frame_ = true;
        LOG(INFO) << "First frame encoded";
    }
    return encoded_frame;
}

std::vector<VideoEncoder::Ability> VideoEncoder::check_encode_abilities(uint32_t width,
                                                                        uint32_t height) {
    auto [device, context, vendor_id, luid] = create_d3d11();
    if (device == nullptr || context == nullptr) {
        return {};
    }
    return do_check_encode_abilities(device, context, vendor_id, luid, width, height);
}

std::vector<VideoEncoder::Ability>
VideoEncoder::check_encode_abilities_with_luid(int64_t luid, uint32_t width, uint32_t height) {
    auto [device, context, vendor_id, _] = create_d3d11_with_luid(luid);
    if (device == nullptr || context == nullptr) {
        return {};
    }
    return do_check_encode_abilities(device, context, vendor_id, luid, width, height);
}

bool VideoEncoder::InitParams::validate() const {
    if (this->width == 0 || this->height == 0 || this->bitrate_bps == 0) {
        return false;
    }
    if (codec_type != lt::VideoCodecType::H264 && codec_type != lt::VideoCodecType::H265) {
        return false;
    }
    return true;
}

} // namespace lt