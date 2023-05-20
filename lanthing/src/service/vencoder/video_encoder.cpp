#include <d3d11_1.h>
#include <wrl/client.h>
#include <g3log/g3log.hpp>
#include <ltlib/times.h>
#include <ltlib/strings.h>
#include "service/vencoder/video_encoder.h"
#include "service/vencoder/nvidia_encoder.h"
#include "service/vencoder/intel_encoder.h"

namespace
{

bool is_black_frame(const lt::svc::VideoEncoder::EncodedFrame& encoded_frame)
{
    if (!encoded_frame.is_keyframe && encoded_frame.size < 1000) {
        return true;
    }
    if (encoded_frame.is_keyframe && encoded_frame.size < 2000) {
        return true;
    }
    return false;
}

std::string backend_to_string(lt::svc::VideoEncoder::Backend backend)
{
    switch (backend) {
    case lt::svc::VideoEncoder::Backend::Unknown:
        return "Unknown";
    case lt::svc::VideoEncoder::Backend::NvEnc:
        return "NvEnc";
    case lt::svc::VideoEncoder::Backend::IntelMediaSDK:
        return "IntelMediaSDK";
    case lt::svc::VideoEncoder::Backend::Amf:
        return "Amf";
    default:
        return "Unknown";
    }
}

} // ÄäÃû¿Õ¼ä

namespace lt
{

namespace svc
{

std::unique_ptr<VideoEncoder> VideoEncoder::create(const InitParams& params)
{
    switch (params.backend) {
    case VideoEncoder::Backend::NvEnc: {
        auto encoder = std::make_unique<NvD3d11Encoder>();
        if (encoder->init(params)) {
            LOG(INFO) << "NvidiaEncoder created";
            return encoder;
        } else {
            LOGF(INFO, "Create NvidiaEncoder(w:%u,h:%u,c:%d) failed", params.width, params.height, params.codec_type);
            return nullptr;
        }
    }
    case VideoEncoder::Backend::IntelMediaSDK: {
        auto encoder = std::make_unique<IntelEncoder>();
        if (encoder->init(params)) {
            LOG(INFO) << "IntelEncoder created";
            return encoder;
        } else {
            LOGF(INFO, "Create IntelEncoder(w:%u,h:%u,c:%d) failed", params.width, params.height, params.codec_type);
            return nullptr;
        }
    }
    default:
        LOG(WARNING) << "Unsupport encoder backend " << backend_to_string(params.backend);
        return nullptr;
    }
}

VideoEncoder::VideoEncoder(void* d3d11_device)
    : d3d11_device_ { d3d11_device }
{
    auto dev = reinterpret_cast<ID3D11Device*>(d3d11_device_);
    dev->AddRef();
}

VideoEncoder::~VideoEncoder()
{
    auto dev = reinterpret_cast<ID3D11Device*>(d3d11_device_);
    dev->Release();
}

VideoEncoder::EncodedFrame VideoEncoder::encode(std::shared_ptr<ltproto::peer2peer::CaptureVideoFrame> input_frame, bool force_idr)
{
    if (input_frame->underlying_type() != ltproto::peer2peer::CaptureVideoFrame_UnderlyingType_DxgiSharedHandle) {
        LOG(FATAL) << "Only support DxgiSharedHandle!";
        return {};
    }
    std::wstring name = ltlib::utf8_to_utf16(input_frame->name());
    Microsoft::WRL::ComPtr<ID3D11Device1> d3d11_1_dev;
    Microsoft::WRL::ComPtr<ID3D11Device> d3d11_dev = reinterpret_cast<ID3D11Device*>(d3d11_device_);
    d3d11_dev.As(&d3d11_1_dev);
    Microsoft::WRL::ComPtr<ID3D11Resource> resource;
    auto hr = d3d11_1_dev->OpenSharedResourceByName(name.c_str(), DXGI_SHARED_RESOURCE_READ, __uuidof(ID3D11Resource), (void**)resource.GetAddressOf());
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
    auto encoded_frame = this->encode_one_frame(texture.Get(), force_idr);
    const int64_t end_encode = ltlib::steady_now_us();

    encoded_frame.is_black_frame = is_black_frame(encoded_frame);
    encoded_frame.start_encode_timestamp_us = start_encode;
    encoded_frame.end_encode_timestamp_us = end_encode;
    encoded_frame.ltframe_id = input_frame->picture_id();
    encoded_frame.capture_timestamp_us = input_frame->capture_timestamp_us();
    encoded_frame.width = input_frame->width();
    encoded_frame.height = input_frame->height();
    mutex->ReleaseSync(0);
    return encoded_frame;
}

bool VideoEncoder::InitParams::validate() const
{
    if (this->context == nullptr
        || this->width == 0
        || this->height == 0
        || this->bitrate_bps == 0) {
        return false;
    }
    if (codec_type != ltrtc::VideoCodecType::H264 && codec_type != ltrtc::VideoCodecType::H265) {
        return false;
    }
    return true;
}

} // namespace svc

} // namespace lt