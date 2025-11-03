/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2025 Zhennan Tu <zhennan.tu@gmail.com>
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

#include "nvfbc_video_capturer.h"

#include <d3d11.h>
#include <dxgi.h>

#include <rtc/rtc.h>

#include <ltlib/logging.h>
#include <ltlib/strings.h>
#include <ltlib/system.h>
#include <ltlib/times.h>

#pragma comment(lib, "d3d9.lib")

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

NvFBCVideoCapturer::NvFBCVideoCapturer(ltlib::Monitor monitor)
    : monitor_{monitor} {}

NvFBCVideoCapturer::~NvFBCVideoCapturer() {}

bool NvFBCVideoCapturer::init() {
    if (!initD3D9()) {
        return false;
    }
    impl_ = std::make_unique<NvFBCLibrary>();
    if (!impl_->load()) {
        LOG(ERR) << "Failed to load NvFBC library";
        return false;
    }
    NvFBCStatusEx status{};
    status.dwVersion = NVFBC_STATUS_VER;
    if (FAILED(impl_->getStatus(&status))) {
        LOG(ERR) << "Failed to get NvFBC status";
        return false;
    }
    if (!status.bIsCapturePossible) {
        LOG(ERR) << "NvFBC capture is not possible";
        return false;
    }
    if (status.bCurrentlyCapturing) {
        LOG(INFO) << "NvFBC is currently capturing";
    }
    DWORD maxDisplayWidth = 0, maxDisplayHeight = 0;
    nvfbc_dx9_ =
        (NvFBCToDx9Vid*)impl_->create(NVFBC_TO_DX9_VID, &maxDisplayWidth, &maxDisplayHeight,
                                      adapter_index_, (void*)d3d9_dev_.Get());
    if (!nvfbc_dx9_) {
        LOG(ERR) << "Failed to create NvFBCToDx9Vid instance";
        return false;
    }
    HRESULT hr = d3d9_dev_->CreateOffscreenPlainSurface(
        2560, 1440, D3DFMT_X8R8G8B8, D3DPOOL_DEFAULT, d3d9_surface_.GetAddressOf(), nullptr);
    if (FAILED(hr)) {
        LOGF(ERR, "Failed to create offscreen plain surface(%u x %u), er:%08x", maxDisplayWidth,
             maxDisplayHeight, hr);
        return false;
    }
    nvfbc_outbuf_.pPrimary = d3d9_surface_.Get();
    NVFBC_TODX9VID_SETUP_PARAMS DX9SetupParams = {};
    DX9SetupParams.dwVersion = NVFBC_TODX9VID_SETUP_PARAMS_V3_VER;
    DX9SetupParams.bWithHWCursor = true;
    DX9SetupParams.bStereoGrab = 0;
    DX9SetupParams.bDiffMap = 0;
    DX9SetupParams.ppBuffer = &nvfbc_outbuf_;
    DX9SetupParams.eMode = NVFBC_TODX9VID_ARGB;
    DX9SetupParams.dwNumBuffers = 1;
    hr = nvfbc_dx9_->NvFBCToDx9VidSetUp(&DX9SetupParams);
    if (FAILED(hr)) {
        LOGF(ERR, "NvFBCToDx9VidSetUp failed, er:%d", hr);
        return false;
    }
    return true;
}

bool NvFBCVideoCapturer::start() {
    // 要不要把上面的挪下来?
    return true;
}

bool NvFBCVideoCapturer::initD3D9() {
    Microsoft::WRL::ComPtr<IDXGIFactory1> dxgi_factory;
    HRESULT hr = CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)dxgi_factory.GetAddressOf());
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
        hr = dxgi_factory->EnumAdapters(static_cast<UINT>(index), adapter.GetAddressOf());
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
        if (adapter_desc.VendorId != 0x10DE) {
            // Not Nvidia
            continue;
        }
        hr = Direct3DCreate9Ex(D3D_SDK_VERSION, d3d9_ex_.GetAddressOf());
        if (FAILED(hr)) {
            LOGF(WARNING, "Adapter %d Direct3DCreate9Ex failed", index);
            continue;
        }
        D3DDISPLAYMODE display_mode;
        hr = d3d9_ex_->GetAdapterDisplayMode(index, &display_mode);
        if (FAILED(hr)) {
            LOGF(WARNING, "Adapter %d GetAdapterDisplayMode failed", index);
            continue;
        }
        display_mode_ = display_mode;
        LOG(INFO) << "Display width:" << display_mode.Width << ", height:" << display_mode.Height;
        // DEBUG ?
        D3DPRESENT_PARAMETERS d3dpp;
        ZeroMemory(&d3dpp, sizeof(d3dpp));
        d3dpp.Windowed = true;
        d3dpp.BackBufferFormat = D3DFMT_X8R8G8B8;

        d3dpp.BackBufferWidth = display_mode.Width;
        d3dpp.BackBufferHeight = display_mode.Height;
        d3dpp.BackBufferCount = 1;
        d3dpp.SwapEffect = D3DSWAPEFFECT_COPY;
        d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
        d3dpp.Flags = D3DPRESENTFLAG_VIDEO;
        DWORD dwBehaviorFlags =
            D3DCREATE_FPU_PRESERVE | D3DCREATE_MULTITHREADED | D3DCREATE_HARDWARE_VERTEXPROCESSING;
        hr = d3d9_ex_->CreateDeviceEx(index, D3DDEVTYPE_HAL, nullptr, dwBehaviorFlags, &d3dpp,
                                      nullptr, d3d9_dev_.GetAddressOf());
        if (FAILED(hr)) {
            LOGF(WARNING, "Adapter %d CreateDeviceEx failed: %#x", index, hr);
            continue;
        }
        adapter_index_ = index;
        luid_ =
            ((uint64_t)adapter_desc.AdapterLuid.HighPart << 32) + adapter_desc.AdapterLuid.LowPart;
        LOGF(INFO, "NvFBCVideoCapturer using adapter(index:%d, %x:%x, %lld)", index,
             adapter_desc.VendorId, adapter_desc.DeviceId, luid_);
        return true;
    }
    return false;
}

std::optional<Capturer::Frame> NvFBCVideoCapturer::capture() {
    NvFBCFrameGrabInfo info{};
    NVFBC_TODX9VID_GRAB_FRAME_PARAMS fbcDX9GrabParams = {0};
    NVFBCRESULT fbcRes = NVFBC_SUCCESS;
    fbcDX9GrabParams.dwVersion = NVFBC_TODX9VID_GRAB_FRAME_PARAMS_V1_VER;
    fbcDX9GrabParams.dwFlags = NVFBC_TODX9VID_NOWAIT;
    fbcDX9GrabParams.eGMode = NVFBC_TODX9VID_SOURCEMODE_SCALE;
    fbcDX9GrabParams.dwTargetWidth = display_mode_.Width;
    fbcDX9GrabParams.dwTargetHeight = display_mode_.Height;
    fbcDX9GrabParams.dwWaitTime = 50;
    fbcDX9GrabParams.pNvFBCFrameGrabInfo = &info;

    fbcRes = nvfbc_dx9_->NvFBCToDx9VidGrabFrame(&fbcDX9GrabParams);
    if (fbcRes != NVFBC_SUCCESS) {
        LOGF(ERR, "NvFBCToDx9VidGrabFrame failed: %d", fbcRes);
        return {};
    }
    Capturer::Frame out_frame{};
    out_frame.data = d3d9_surface_.Get();
    out_frame.capture_timestamp_us = ltlib::steady_now_us();
    return out_frame;
}

std::optional<CursorInfo> NvFBCVideoCapturer::cursorInfo() {
    return cursor_info_;
}

void NvFBCVideoCapturer::doneWithFrame() {}

void NvFBCVideoCapturer::waitForVBlank() {
    d3d9_dev_->WaitForVBlank(0);
}

Capturer::Backend NvFBCVideoCapturer::backend() const {
    return Backend::Dxgi;
}

int64_t NvFBCVideoCapturer::luid() {
    return luid_;
}

void* NvFBCVideoCapturer::device() {
    return d3d9_dev_.Get();
}

void* NvFBCVideoCapturer::deviceContext() {
    return d3d9_dev_.Get();
}

uint32_t NvFBCVideoCapturer::vendorID() {
    return 0x10DE;
}

bool NvFBCVideoCapturer::defaultOutput() {
    return true;
}

bool NvFBCVideoCapturer::setCaptureFormat(CaptureFormat format) {
    if (format == capture_foramt_) {
        return true;
    }
    capture_foramt_ = format;
    switch (format) {
    case CaptureFormat::D3D11_BGRA:
    case CaptureFormat::MEM_I420:
        return true;
    default:
        LOG(ERR) << "NvFBCVideoCapturer: Unknown CaptureFormat " << (int)format;
        return false;
    }
}

} // namespace video

} // namespace lt