#include "gpu_capability.h"

#include <d3d11.h>
#include <d3d11_1.h>
#include <dwmapi.h>
#include <dxgi1_5.h>

#include "ltlib/locale.h"

#include <g3log/g3log.hpp>

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
    IDXGIFactory5* dxgi_factory = nullptr;
    hr = CreateDXGIFactory(__uuidof(IDXGIFactory5), (void**)&dxgi_factory);
    if (FAILED(hr)) {
        LOGF(WARNING, "fail to create dxgi factory, er:%08x", hr);
        return false;
    }

    UINT i = 0;
    IDXGIAdapter* pAdapter;
    std::vector<IDXGIAdapter*> vAdapters;
    while (dxgi_factory->EnumAdapters(i, &pAdapter) != DXGI_ERROR_NOT_FOUND) {
        vAdapters.push_back(pAdapter);
        ++i;
    }
    IDXGISwapChain4* swap_chain = nullptr;
    UINT flag = D3D11_CREATE_DEVICE_VIDEO_SUPPORT;
#ifdef _DEBUG
    flag |= D3D11_CREATE_DEVICE_DEBUG;
#endif
    for (auto& adapter : vAdapters) {
        Ability ability;
        DXGI_ADAPTER_DESC desc;
        adapter->GetDesc(&desc);
        ability.vendor = desc.VendorId;
        ability.desc = ltlib::wideCharToUtf8(desc.Description);
        ability.device_id = desc.DeviceId;
        ability.driver = "0.0.0.0";
        ability.video_memory_mb = desc.DedicatedVideoMemory / 1024 / 1024;
        ability.luid = ((uint64_t)desc.AdapterLuid.HighPart << 32) + desc.AdapterLuid.LowPart;

        ID3D11Device* d3d11_dev = nullptr;
        ID3D11DeviceContext* d3d11_ctx = nullptr;
        hr = D3D11CreateDevice(adapter, D3D_DRIVER_TYPE_UNKNOWN, nullptr, flag, nullptr, 0,
                               D3D11_SDK_VERSION, &d3d11_dev, nullptr, &d3d11_ctx);
        if (FAILED(hr)) {
            LOGF(WARNING, "fail to create d3d11 device on %s, err:%08lx", ability.to_str().c_str(),
                 hr);
            continue;
        }
        ID3D11VideoDevice* video_device = nullptr;
        auto hr = d3d11_dev->QueryInterface(__uuidof(ID3D11VideoDevice), (void**)&video_device);
        if (FAILED(hr)) {
            LOGF(WARNING, "failed to get ID3D11VideoDevice on %s, hr:%08lx", hr,
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
            ability.formats.push_back(Format::H264_NV12);
        }
        supported = false;
        guid = D3D11_DECODER_PROFILE_HEVC_VLD_MAIN;
        format = DXGI_FORMAT_NV12;
        hr = video_device->CheckVideoDecoderFormat(&guid, format, &supported);
        if (!FAILED(hr) && supported) {
            ability.formats.push_back(Format::H265_NV12);
        }
        // TODO: check DXGI_FORMAT_AYUV;
        abilities_.push_back(ability);

        video_device->Release();
        d3d11_dev->Release();
        d3d11_ctx->Release();
    }
    dxgi_factory->Release();

    return true;
}
} // namespace lt
