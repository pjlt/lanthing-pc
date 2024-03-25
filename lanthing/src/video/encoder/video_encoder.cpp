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

#include <d3d11_1.h>
#include <dxgi1_3.h>
#include <wrl/client.h>

#include <ltlib/logging.h>

#include <ltlib/strings.h>
#include <ltlib/times.h>

#include "amd_encoder.h"
#include "intel_encoder.h"
#include "nvidia_encoder.h"
#include "params_helper.h"
#include "video_encoder.h"

#if defined(LT_WINDOWS)
#include "openh264_encoder.h"
#endif // defined(LT_WINDOWS)

using Microsoft::WRL::ComPtr;

namespace {

constexpr uint32_t kAMDVendorID = 0x1002;
constexpr uint32_t kIntelVendorID = 0x8086;
constexpr uint32_t kNvidiaVendorID = 0x10DE;

auto createD3d11()
    -> std::tuple<ComPtr<ID3D11Device>, ComPtr<ID3D11DeviceContext>, uint32_t, int64_t> {
    // TODO: 遍历每个GPU
    uint32_t vendor_id = 0;
    int64_t luid = 0;
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    ComPtr<IDXGIFactory2> dxgi_factory;
    auto hr = CreateDXGIFactory2(0, __uuidof(IDXGIFactory2), (void**)dxgi_factory.GetAddressOf());
    if (FAILED(hr)) {
        LOGF(ERR, "Failed to create dxgi factory2, hr:0x%08x", hr);
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
        LOGF(ERR, "fail to create d3d11 device, err:%08lx", hr);
        return {nullptr, nullptr, vendor_id, luid};
    }
    LOGF(INFO, "D3D11Device(index:0, %x:%x) created", desc.VendorId, desc.DeviceId);
    return {device, context, vendor_id, luid};
}

auto createD3D11WithLuid(int64_t luid)
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
        LOGF(ERR, "fail to create d3d11 device, err:%08lx", hr);
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
        return createD3D11WithLuid(luid.value());
    }
    else {
        return createD3d11();
    }
}

std::unique_ptr<lt::video::Encoder> doCreateHard(const lt::video::Encoder::InitParams& params) {
    using namespace lt::video;
    EncodeParamsHelper params_helper{params.device,     params.context,     params.luid,
                                     params.codec_type, params.width,       params.height,
                                     params.freq,       params.bitrate_bps, true};
    switch (params.vendor_id) {
    case kNvidiaVendorID:
    {
        auto encoder = NvD3d11Encoder::create(params_helper);
        if (encoder) {
            LOG(INFO) << "NvidiaEncoder created";
            return encoder;
        }
        else {
            LOGF(INFO, "Create NvidiaEncoder(w:%u,h:%u,c:%u) failed", params.width, params.height,
                 params.codec_type);
            return nullptr;
        }
    }
    case kIntelVendorID:
    {
        auto encoder = IntelEncoder::create(params_helper);
        if (encoder) {
            LOG(INFO) << "IntelEncoder created";
            return encoder;
        }
        else {
            LOGF(INFO, "Create IntelEncoder(w:%u,h:%u,c:%u) failed", params.width, params.height,
                 params.codec_type);
            return nullptr;
        }
    }
    case kAMDVendorID:
    {
        auto encoder = AmdEncoder::create(params_helper);
        if (encoder) {
            LOG(INFO) << "AmdEncoder created";
            return encoder;
        }
        else {
            LOGF(INFO, "Create AmdEncoder(w:%u,h:%u,c:%u) failed", params.width, params.height,
                 params.codec_type);
            return nullptr;
        }
    }
    default:
        LOGF(WARNING, "Unsupport gpu vendor %#x", params.vendor_id);
        return nullptr;
    }
}

} // namespace

namespace lt {

namespace video {

std::unique_ptr<Encoder> Encoder::createHard(const InitParams& params) {
    if (!params.validate()) {
        LOG(ERR) << "Create Hard VideoEncoder failed: invalid parameters";
        return nullptr;
    }
    return doCreateHard(params);
}

std::unique_ptr<Encoder> Encoder::createSoft(const InitParams& params) {
#if defined(LT_WINDOWS)
    EncodeParamsHelper params_helper{params.device,     params.context,     params.luid,
                                     params.codec_type, params.width,       params.height,
                                     params.freq,       params.bitrate_bps, true};
    return OpenH264Encoder::create(params_helper);
#else  // defined(LT_WINDOWS)
    (void)params;
    return nullptr;
#endif // defined(LT_WINDOWS)
}

bool Encoder::needKeyframe() {
    return request_keyframe_.exchange(false);
}

void Encoder::requestKeyframe() {
    request_keyframe_ = true;
}

std::shared_ptr<ltproto::client2worker::VideoFrame>
Encoder::encode(const Capturer::Frame& input_frame) {
    const int64_t start_encode = ltlib::steady_now_us();
    auto encoded_frame = this->encodeFrame(input_frame.data);
    const int64_t end_encode = ltlib::steady_now_us();
    if (encoded_frame == nullptr) {
        return nullptr;
    }
    encoded_frame->set_capture_timestamp_us(input_frame.capture_timestamp_us);
    encoded_frame->set_start_encode_timestamp_us(start_encode);
    encoded_frame->set_end_encode_timestamp_us(end_encode);
    encoded_frame->set_picture_id(frame_id_++);
    encoded_frame->set_width(width());
    encoded_frame->set_height(height());
    if (!first_frame_) {
        first_frame_ = true;
        LOG(INFO) << "First frame encoded";
    }
    if (encoded_frame->is_keyframe()) {
        LOG(DEBUG) << "SEND KEY FRAME";
    }
    return encoded_frame;
}

bool Encoder::doneFrame1() const {
    return true;
}

bool Encoder::doneFrame2() const {
    return false;
}

bool Encoder::InitParams::validate() const {
    if (this->width == 0 || this->height == 0 || this->bitrate_bps == 0 ||
        this->device == nullptr || this->context == nullptr || this->freq == 0 ||
        this->freq > 240) {
        return false;
    }
    if (codec_type != lt::VideoCodecType::H264_420 && codec_type != lt::VideoCodecType::H265_420) {
        return false;
    }
    return true;
}

} // namespace video

} // namespace lt
