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

#include "dxgi_video_capturer.h"

#include <d3d11.h>
#include <dxgi.h>

#include <rtc/rtc.h>

#include <ltlib/logging.h>
#include <ltlib/strings.h>
#include <ltlib/system.h>
#include <ltlib/times.h>

#if 0
static int64_t point1;
static int64_t point2;
static int64_t point3;
static int64_t point4;
static int64_t point5;
static int64_t point6;
static int64_t point7;
#define RECORD_T(x) x = ltlib::steady_now_us();
#define LOG_RECORD_T()                                                                             \
    LOGF(INFO, "RECORD_T %lld,%lld,%lld,%lld,%lld,%lld,%lld", point1, point2, point3, point4,      \
         point5, point6, point7)
#else
#define RECORD_T(X)
#define LOG_RECORD_T()
#endif

using namespace Microsoft::WRL;

namespace lt {

namespace video {

static CursorFormat toCursorFormat(UINT type) {
    switch (type) {
    case DXGI_OUTDUPL_POINTER_SHAPE_TYPE_MONOCHROME:
        return CursorFormat::MonoChrome;
    case DXGI_OUTDUPL_POINTER_SHAPE_TYPE_COLOR:
        return CursorFormat::Color;
    case DXGI_OUTDUPL_POINTER_SHAPE_TYPE_MASKED_COLOR:
        return CursorFormat::MaskedColor;
    default:
        LOG(FATAL) << "Unknown Pointer shape type " << type;
        return CursorFormat::Unknown;
    }
}

static std::string rotationString(DXGI_MODE_ROTATION rotation) {
    switch (rotation) {
    case DXGI_MODE_ROTATION_UNSPECIFIED:
        return "DXGI_MODE_ROTATION_UNSPECIFIED";
    case DXGI_MODE_ROTATION_IDENTITY:
        return "DXGI_MODE_ROTATION_IDENTITY";
    case DXGI_MODE_ROTATION_ROTATE90:
        return "DXGI_MODE_ROTATION_ROTATE90";
    case DXGI_MODE_ROTATION_ROTATE180:
        return "DXGI_MODE_ROTATION_ROTATE180";
    case DXGI_MODE_ROTATION_ROTATE270:
        return "DXGI_MODE_ROTATION_ROTATE270";
    default:
        return "DXGI_MODE_ROTATION_" + std::to_string((int)rotation);
    }
}

static std::string colorspaceString(DXGI_COLOR_SPACE_TYPE colorspace) {
    int value = (int)colorspace;
    if (value < 0 || value > (int)DXGI_COLOR_SPACE_YCBCR_STUDIO_G24_TOPLEFT_P2020) {
        return "DXGI_COLOR_SPACE_TYPE_" + std::to_string(value);
    }
    static const char* names[] = {
        "DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709",
        "DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709",
        "DXGI_COLOR_SPACE_RGB_STUDIO_G22_NONE_P709",
        "DXGI_COLOR_SPACE_RGB_STUDIO_G22_NONE_P2020",
        "DXGI_COLOR_SPACE_RESERVED",
        "DXGI_COLOR_SPACE_YCBCR_FULL_G22_NONE_P709_X601",
        "DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_LEFT_P601",
        "DXGI_COLOR_SPACE_YCBCR_FULL_G22_LEFT_P601",
        "DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_LEFT_P709",
        "DXGI_COLOR_SPACE_YCBCR_FULL_G22_LEFT_P709",
        "DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_LEFT_P2020",
        "DXGI_COLOR_SPACE_YCBCR_FULL_G22_LEFT_P2020",
        "DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020",
        "DXGI_COLOR_SPACE_YCBCR_STUDIO_G2084_LEFT_P2020",
        "DXGI_COLOR_SPACE_RGB_STUDIO_G2084_NONE_P2020",
        "DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_TOPLEFT_P2020",
        "DXGI_COLOR_SPACE_YCBCR_STUDIO_G2084_TOPLEFT_P2020",
        "DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P2020",
        "DXGI_COLOR_SPACE_YCBCR_STUDIO_GHLG_TOPLEFT_P2020",
        "DXGI_COLOR_SPACE_YCBCR_FULL_GHLG_TOPLEFT_P2020",
        "DXGI_COLOR_SPACE_RGB_STUDIO_G24_NONE_P709",
        "DXGI_COLOR_SPACE_RGB_STUDIO_G24_NONE_P2020",
        "DXGI_COLOR_SPACE_YCBCR_STUDIO_G24_LEFT_P709",
        "DXGI_COLOR_SPACE_YCBCR_STUDIO_G24_LEFT_P2020",
        "DXGI_COLOR_SPACE_YCBCR_STUDIO_G24_TOPLEFT_P2020",
    };
    return names[value];
}

DxgiVideoCapturer::DxgiVideoCapturer(ltlib::Monitor monitor)
    : impl_{std::make_unique<DUPLICATIONMANAGER>()}
    , monitor_{monitor} {}

DxgiVideoCapturer::~DxgiVideoCapturer() {}

bool DxgiVideoCapturer::init() {
    if (!initD3D11()) {
        return false;
    }
    if (!impl_->InitDupl(d3d11_dev_.Get(), monitor_)) {
        LOG(ERR) << "Failed to init DUPLICATIONMANAGER";
        return false;
    }
    DXGI_OUTPUT_DESC1 desc = impl_->GetOutputDesc1();
    LOGF(INFO,
         "Current dxgi output desc DeviceName: %s, Resolution: %dx%d, "
         "DesktopCoordinates :{top:%d, bottom:%d, left:%d, right:%d}, "
         "AttachedToDesktop: %d, Rotation: %s, BitsPerColor: %u, ColorSpace: %s, "
         "RedPrimary: {%f, %f}, GreenPrimary: {%f, %f}, BluePrimary: {%f, %f}, "
         "WhitePoint: {%f, %f}, MinLuminance: %f, MaxLuminance: %f, "
         "MaxFullFrameLuminance: %f",
         ltlib::utf16To8(desc.DeviceName).c_str(),
         desc.DesktopCoordinates.right - desc.DesktopCoordinates.left,
         desc.DesktopCoordinates.bottom - desc.DesktopCoordinates.top, desc.DesktopCoordinates.top,
         desc.DesktopCoordinates.bottom, desc.DesktopCoordinates.left,
         desc.DesktopCoordinates.right, desc.AttachedToDesktop,
         rotationString(desc.Rotation).c_str(), desc.BitsPerColor,
         colorspaceString(desc.ColorSpace).c_str(), desc.RedPrimary[0], desc.RedPrimary[1],
         desc.GreenPrimary[0], desc.GreenPrimary[1], desc.BluePrimary[0], desc.BluePrimary[1],
         desc.WhitePoint[0], desc.WhitePoint[1], desc.MinLuminance, desc.MaxLuminance,
         desc.MaxFullFrameLuminance);
    // 这对吗?
    if (desc.ColorSpace == DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P2020) {
        LOG(WARNING) << "BT2020 unsupported yet, treat as BT709";
        color_primaries_ = ColorPrimaries::BT709;
    }
    else if (desc.ColorSpace == DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709) {
        color_primaries_ = ColorPrimaries::BT709;
    }
    else {
        LOG(WARNING) << "Unsupported color space " << colorspaceString(desc.ColorSpace);
        color_primaries_ = ColorPrimaries::Undefined;
    }
    return true;
}

bool DxgiVideoCapturer::start() {
    return true;
}

bool DxgiVideoCapturer::initD3D11() {
    HRESULT hr = CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)dxgi_factory_.GetAddressOf());
    if (FAILED(hr)) {
        LOGF(ERR, "Failed to create dxgi factory, er:%08x", hr);
        return false;
    }
    bool out_of_bound = false;
    int32_t index = -1;
    while (true) {
        index += 1;
        Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
        DXGI_ADAPTER_DESC adapter_desc;
        hr = dxgi_factory_->EnumAdapters(static_cast<UINT>(index), adapter.GetAddressOf());
        if (hr == DXGI_ERROR_NOT_FOUND) {
            LOGF(WARNING, "Failed to find no %d adapter", index);
            out_of_bound = true;
            break;
        }
        if (hr == DXGI_ERROR_INVALID_CALL) {
            LOG(FATAL) << "DXGI Factory == nullptr";
            return false;
        }
        hr = adapter->GetDesc(&adapter_desc);
        if (FAILED(hr)) {
            LOGF(WARNING, "Adapter %d GetDesc failed", index);
            continue;
        }
        vendor_id_ = adapter_desc.VendorId;
        UINT flag = 0;
#ifdef _DEBUG
        flag |= D3D11_CREATE_DEVICE_DEBUG;
#endif
        hr = D3D11CreateDevice(adapter.Get(), D3D_DRIVER_TYPE_UNKNOWN, nullptr, flag, nullptr, 0,
                               D3D11_SDK_VERSION, d3d11_dev_.GetAddressOf(), nullptr,
                               d3d11_ctx_.GetAddressOf());
        if (FAILED(hr)) {
            LOGF(WARNING, "Adapter(%x:%x) failed to create d3d11 device, err:%08lx",
                 adapter_desc.VendorId, adapter_desc.DeviceId, hr);
            continue;
        }
        luid_ =
            ((uint64_t)adapter_desc.AdapterLuid.HighPart << 32) + adapter_desc.AdapterLuid.LowPart;
        LOGF(INFO, "DxgiVideoCapturer using adapter(index:%d, %x:%x, %lld)", index,
             adapter_desc.VendorId, adapter_desc.DeviceId, luid_);
        return true;
    }
    return false;
}

std::optional<Capturer::Frame> DxgiVideoCapturer::capture() {
    FRAME_DATA frame;
    bool timeout = false;
    RECORD_T(point1);
    auto hr = impl_->GetFrame(&frame, &timeout);
    if (hr == DUPL_RETURN::DUPL_RETURN_SUCCESS && !timeout) {
        RECORD_T(point2);
        Capturer::Frame out_frame{};
        if (capture_foramt_ == CaptureFormat::D3D11_BGRA) {
            out_frame.data = frame.Frame;
        }
        else {
            auto mem_data = toI420(frame.Frame);
            if (mem_data == nullptr) {
                return {};
            }
            out_frame.data = mem_data;
        }
        out_frame.capture_timestamp_us = ltlib::steady_now_us();
        saveCursorInfo(&frame.FrameInfo);
        return out_frame;
    }
    return {};
}

std::optional<CursorInfo> DxgiVideoCapturer::cursorInfo() {
    return cursor_info_;
}

uint8_t* DxgiVideoCapturer::toI420(ID3D11Texture2D* frame) {
    D3D11_TEXTURE2D_DESC desc{};
    frame->GetDesc(&desc);
    if (stage_texture_ == nullptr) {
        D3D11_TEXTURE2D_DESC desc2{};
        desc2.Width = desc.Width;
        desc2.Height = desc.Height;
        desc2.Format = desc.Format;
        desc2.ArraySize = 1;
        desc2.BindFlags = 0;
        desc2.MiscFlags = 0;
        desc2.SampleDesc.Count = 1;
        desc2.SampleDesc.Quality = 0;
        desc2.MipLevels = 1;
        desc2.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        desc2.Usage = D3D11_USAGE_STAGING;
        HRESULT hr = d3d11_dev_->CreateTexture2D(&desc2, nullptr, stage_texture_.GetAddressOf());
        if (FAILED(hr)) {
            LOGF(ERR, "Create staging texture2d failed: %#x", hr);
            return nullptr;
        }
        if (stage_texture_ == nullptr) {
            LOG(ERR) << "Create staging texture2d failed, texture == nullptr";
            return nullptr;
        }
    }
    d3d11_ctx_->CopyResource(stage_texture_.Get(), frame);
    D3D11_MAPPED_SUBRESOURCE mapped{};
    UINT subres = D3D11CalcSubresource(0, 0, 0);
    HRESULT hr = d3d11_ctx_->Map(stage_texture_.Get(), subres, D3D11_MAP_READ, 0, &mapped);
    if (FAILED(hr)) {
        LOGF(ERR, "ID3D11DeviceContext::Map failed %#x", hr);
        return nullptr;
    }
    size_t need_size = desc.Width * desc.Height * 3 / 2;
    if (mem_buff_.size() < need_size) {
        mem_buff_.resize(need_size);
    }
    // 其实是libyuv，但是rtc.dll已经集成了，就不再单独编译一份libyuv，二次导出即可
    int width = static_cast<int>(desc.Width);
    int height = static_cast<int>(desc.Height);
    int ret = rtc::ARGBToI420(reinterpret_cast<BYTE*>(mapped.pData), mapped.RowPitch,
                              mem_buff_.data(), width, mem_buff_.data() + width * height, width / 2,
                              mem_buff_.data() + width * height + width * height / 4, width / 2,
                              width, height);
    d3d11_ctx_->Unmap(stage_texture_.Get(), subres);
    if (ret != 0) {
        LOG(ERR) << "rtc::ARGBToI420 failed " << ret;
        return nullptr;
    }
    return mem_buff_.data();
}

void DxgiVideoCapturer::saveCursorInfo(DXGI_OUTDUPL_FRAME_INFO* frame_info) {
    if (frame_info->LastMouseUpdateTime.QuadPart <= 0) {
        return;
    }
    CursorInfo info{};
    info.x = frame_info->PointerPosition.Position.x;
    info.y = frame_info->PointerPosition.Position.y;
    info.visible = frame_info->PointerPosition.Visible != FALSE;
    if (frame_info->PointerShapeBufferSize > 0) {
        DXGI_OUTDUPL_POINTER_SHAPE_INFO shape_info{};
        std::vector<uint8_t> shape_data;
        shape_data.resize(frame_info->PointerShapeBufferSize);
        if (impl_->GetPointerShape(shape_info, shape_data)) {
            info.hot_x = static_cast<uint16_t>(shape_info.HotSpot.x);
            info.hot_y = static_cast<uint16_t>(shape_info.HotSpot.y);
            info.format = toCursorFormat(shape_info.Type);
            info.w = shape_info.Width;
            info.h = shape_info.Height;
            info.pitch = static_cast<uint16_t>(shape_info.Pitch);
            info.data = std::move(shape_data);
        }
    }
    cursor_info_ = info;
}

void DxgiVideoCapturer::doneWithFrame() {
    impl_->DoneWithFrame();
}

void DxgiVideoCapturer::waitForVBlank() {
    impl_->WaitForVBlank();
}

Capturer::Backend DxgiVideoCapturer::backend() const {
    return Backend::Dxgi;
}

int64_t DxgiVideoCapturer::luid() {
    return luid_;
}

void* DxgiVideoCapturer::device() {
    return d3d11_dev_.Get();
}

void* DxgiVideoCapturer::deviceContext() {
    return d3d11_ctx_.Get();
}

uint32_t DxgiVideoCapturer::vendorID() {
    return vendor_id_;
}

bool DxgiVideoCapturer::defaultOutput() {
    return impl_->DefaultOutput();
}

bool DxgiVideoCapturer::setCaptureFormat(CaptureFormat format) {
    if (format == capture_foramt_) {
        return true;
    }
    capture_foramt_ = format;
    switch (format) {
    case CaptureFormat::D3D11_BGRA:
    case CaptureFormat::MEM_I420:
        return true;
    default:
        LOG(ERR) << "DxgiVideoCapturer: Unknown CaptureFormat " << (int)format;
        return false;
    }
}

ColorPrimaries DxgiVideoCapturer::colorPrimaries() {
    return color_primaries_;
}

} // namespace video

} // namespace lt