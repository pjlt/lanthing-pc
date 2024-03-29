/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2024 Zhennan Tu <zhennan.tu@gmail.com>
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

#include "d3d11_video_device.h"

#include <d3d11_1.h>
#include <dxgi1_5.h>

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include <ltlib/logging.h>
#include <ltlib/strings.h>
#include <transport/transport.h>

namespace {

struct Ability {
    uint64_t luid = 0;
    uint32_t vendor = 0;
    std::string desc;
    uint32_t device_id = 0;
    uint32_t video_memory_mb = 0;
    std::string driver;
    std::vector<lt::VideoCodecType> codecs;
    Microsoft::WRL::ComPtr<ID3D11Device> device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;

    std::string to_str() const;
};

std::string Ability::to_str() const {
    char buf[1024] = {0};
    snprintf(buf, sizeof(buf) - 1, "%04x-%s-%04x-%s-%uMB-%s", vendor, desc.c_str(), device_id,
             driver.c_str(), video_memory_mb, std::to_string(luid).c_str());
    return buf;
}

} // namespace

namespace lt {

namespace plat {

using namespace Microsoft::WRL;

std::unique_ptr<D3D11VideoDevice> D3D11VideoDevice::create(VideoCodecType codec) {
    std::unique_ptr<D3D11VideoDevice> dev{new D3D11VideoDevice{codec}};
    if (!dev->init()) {
        return nullptr;
    }
    return dev;
}
D3D11VideoDevice::D3D11VideoDevice(VideoCodecType codec)
    : codec_{codec} {}

D3D11VideoDevice::~D3D11VideoDevice() {}

void* D3D11VideoDevice::device() {
    return device_.Get();
}
void* D3D11VideoDevice::context() {
    return context_.Get();
}

bool D3D11VideoDevice::init() {
    std::map<uint32_t, Ability> abilities;
    ComPtr<IDXGIFactory5> dxgi_factory;
    HRESULT hr = CreateDXGIFactory(__uuidof(IDXGIFactory5), (void**)dxgi_factory.GetAddressOf());
    if (FAILED(hr)) {
        LOGF(ERR, "Failed to create dxgi factory, er:%08x", hr);
        return false;
    }

    UINT i = 0;
    ComPtr<IDXGIAdapter> tmp_adapter;
    std::vector<ComPtr<IDXGIAdapter>> adapters;
    while (dxgi_factory->EnumAdapters(i, tmp_adapter.GetAddressOf()) != DXGI_ERROR_NOT_FOUND) {
        adapters.push_back(tmp_adapter);
        tmp_adapter = nullptr; // ??
        ++i;
    }
    UINT flag = 0;
    if (isHard(codec_)) {
        flag = D3D11_CREATE_DEVICE_VIDEO_SUPPORT;
    }
#ifdef _DEBUG
    flag |= D3D11_CREATE_DEVICE_DEBUG;
#endif
    for (auto& adapter : adapters) {
        Ability ability;
        DXGI_ADAPTER_DESC desc;
        adapter->GetDesc(&desc);
        ability.vendor = desc.VendorId;
        ability.desc = ltlib::utf16To8(desc.Description);
        ability.device_id = desc.DeviceId;
        ability.driver = "0.0.0.0";
        ability.video_memory_mb = static_cast<uint32_t>(desc.DedicatedVideoMemory / 1024 / 1024);
        ability.luid = ((uint64_t)desc.AdapterLuid.HighPart << 32) + desc.AdapterLuid.LowPart;

        ComPtr<ID3D11Device> d3d11_dev;
        ComPtr<ID3D11DeviceContext> d3d11_ctx;
        hr = D3D11CreateDevice(adapter.Get(), D3D_DRIVER_TYPE_UNKNOWN, nullptr, flag, nullptr, 0,
                               D3D11_SDK_VERSION, d3d11_dev.GetAddressOf(), nullptr,
                               d3d11_ctx.GetAddressOf());
        if (FAILED(hr)) {
            LOGF(ERR, "Failed to create d3d11 device on %s, err:%08lx", ability.to_str().c_str(),
                 hr);
            continue;
        }
        ability.device = d3d11_dev;
        ability.context = d3d11_ctx;
        if (isHard(codec_)) {
            ComPtr<ID3D11VideoDevice> video_device;
            hr = d3d11_dev->QueryInterface(__uuidof(ID3D11VideoDevice),
                                           (void**)video_device.GetAddressOf());
            if (FAILED(hr)) {
                LOGF(ERR, "Failed to get ID3D11VideoDevice on %s, hr:%08lx",
                     ability.to_str().c_str(), hr);
                continue;
            }
            GUID guid;
            DXGI_FORMAT format;
            guid = D3D11_DECODER_PROFILE_H264_VLD_NOFGT;
            format = DXGI_FORMAT_NV12;
            BOOL supported = false;
            hr = video_device->CheckVideoDecoderFormat(&guid, format, &supported);
            if (!FAILED(hr) && supported) {
                ability.codecs.push_back(lt::VideoCodecType::H264);
            }
            supported = false;
            guid = D3D11_DECODER_PROFILE_HEVC_VLD_MAIN;
            format = DXGI_FORMAT_NV12;
            hr = video_device->CheckVideoDecoderFormat(&guid, format, &supported);
            if (!FAILED(hr) && supported) {
                ability.codecs.push_back(lt::VideoCodecType::H265);
            }
        }
        ability.codecs.push_back(lt::VideoCodecType::H264_420_SOFT);
        abilities.emplace(ability.video_memory_mb, ability);
    }
    if (abilities.empty()) {
        return false;
    }
    for (auto& entry : abilities) {
        for (auto codec : entry.second.codecs) {
            if (codec == codec_) {
                device_ = entry.second.device;
                context_ = entry.second.context;
                LOG(INFO) << "Using " << entry.second.to_str();
                return true;
            }
        }
    }
    LOG(ERR) << "No decode ability for " << toString(codec_);
    return false;
}

} // namespace plat

} // namespace lt