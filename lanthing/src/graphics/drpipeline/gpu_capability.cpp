#include "gpu_capability.h"

#define INITGUID

#include <d3d11.h>
#include <d3d11_1.h>
#include <dwmapi.h>
#include <dxgi1_5.h>
#include <wrl/client.h>

#include <g3log/g3log.hpp>

#include <ltlib/strings.h>

using namespace Microsoft::WRL;

namespace lt {

std::string GpuInfo::Ability::to_str() const {
    char buf[1024] = {0};
    snprintf(buf, sizeof(buf) - 1, "%04x-%s-%04x-%s-%uMB", vendor, desc.c_str(), device_id,
             driver.c_str(), video_memory_mb);
    return buf;
}

bool GpuInfo::init() {
    HRESULT hr;
    // 用最高版本
    ComPtr<IDXGIFactory5> dxgi_factory;
    hr = CreateDXGIFactory(__uuidof(IDXGIFactory5), (void**)dxgi_factory.GetAddressOf());
    if (FAILED(hr)) {
        LOGF(WARNING, "Failed to create dxgi factory, er:%08x", hr);
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
    // IDXGISwapChain4* swap_chain = nullptr;
    UINT flag = D3D11_CREATE_DEVICE_VIDEO_SUPPORT;
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
            LOGF(WARNING, "Failed to create d3d11 device on %s, err:%08lx",
                 ability.to_str().c_str(), hr);
            continue;
        }
        ComPtr<ID3D11VideoDevice> video_device;
        hr = d3d11_dev->QueryInterface(__uuidof(ID3D11VideoDevice),
                                       (void**)video_device.GetAddressOf());
        if (FAILED(hr)) {
            LOGF(WARNING, "Failed to get ID3D11VideoDevice on %s, hr:%08lx", hr,
                 ability.to_str().c_str());
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
        // TODO: check DXGI_FORMAT_AYUV;
        if (!ability.codecs.empty()) {
            abilities_.push_back(ability);
        }
    }
    return true;
}
} // namespace lt
