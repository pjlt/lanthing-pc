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

DxgiVideoCapturer::DxgiVideoCapturer()
    : impl_{std::make_unique<DUPLICATIONMANAGER>()} {}

DxgiVideoCapturer::~DxgiVideoCapturer() {}

bool DxgiVideoCapturer::init() {
    if (!initD3D11()) {
        return false;
    }
    if (!ltlib::setThreadDesktop()) {
        LOG(ERR) << "DxgiVideoCapturer::init setThreadDesktop failed";
        return false;
    }
    if (!impl_->InitDupl(d3d11_dev_.Get(), 0)) {
        LOG(ERR) << "Failed to init DUPLICATIONMANAGER";
        return false;
    }
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

std::optional<VideoCapturer::Frame> DxgiVideoCapturer::capture() {
    FRAME_DATA frame;
    bool timeout = false;
    RECORD_T(point1);
    auto hr = impl_->GetFrame(&frame, &timeout);
    if (hr == DUPL_RETURN::DUPL_RETURN_SUCCESS && !timeout) {
        RECORD_T(point2);
        VideoCapturer::Frame out_frame{};
        out_frame.data = frame.Frame;
        out_frame.capture_timestamp_us = ltlib::steady_now_us();
        return out_frame;
    }
    return {};
}

void DxgiVideoCapturer::doneWithFrame() {
    impl_->DoneWithFrame();
}

void DxgiVideoCapturer::waitForVBlank() {
    impl_->WaitForVBlank();
}

VideoCapturer::Backend DxgiVideoCapturer::backend() const {
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

} // namespace lt