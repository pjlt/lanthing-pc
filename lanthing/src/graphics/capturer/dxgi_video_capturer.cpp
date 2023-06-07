#include "dxgi_video_capturer.h"

#include <d3d11.h>
#include <dxgi.h>

#include <g3log/g3log.hpp>

#include <ltlib/strings.h>
#include <ltlib/times.h>

namespace lt {

DxgiVideoCapturer::DxgiVideoCapturer()
    : impl_{std::make_unique<DUPLICATIONMANAGER>()} {}

DxgiVideoCapturer::~DxgiVideoCapturer() {}

bool DxgiVideoCapturer::pre_init() {
    if (!init_d3d11()) {
        return false;
    }
    if (!impl_->InitDupl(d3d11_dev_.Get(), 0)) {
        LOG(WARNING) << "Failed to init DUPLICATIONMANAGER";
        return false;
    }
    return true;
}

bool DxgiVideoCapturer::init_d3d11() {
    // 第一块显卡
    const uint32_t index = 0;
    HRESULT hr;
    do {
        hr = CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)dxgi_factory_.GetAddressOf());
        if (FAILED(hr)) {
            LOGF(WARNING, "Failed to create dxgi factory, er:%08x", hr);
            break;
        }
        IDXGIAdapter1* adapter = nullptr;
        DXGI_ADAPTER_DESC1 adapter_desc;
        hr = dxgi_factory_->EnumAdapters1(index, &adapter);
        if (hr == DXGI_ERROR_NOT_FOUND) {
            LOGF(WARNING, "Failed to find no %u adapter", index);
            break;
        }
        hr = adapter->GetDesc1(&adapter_desc);
        if (FAILED(hr)) {
            break;
        }
        UINT flag = 0;
#ifdef _DEBUG
        flag |= D3D11_CREATE_DEVICE_DEBUG;
#endif
        hr = D3D11CreateDevice(adapter, D3D_DRIVER_TYPE_UNKNOWN, nullptr, flag, nullptr, 0,
                               D3D11_SDK_VERSION, d3d11_dev_.GetAddressOf(), nullptr,
                               d3d11_ctx_.GetAddressOf());
        if (FAILED(hr)) {
            LOGF(WARNING, "fail to create d3d11 device, err:%08lx", hr);
            break;
        }
    } while (false);

    return SUCCEEDED(hr);
}

std::shared_ptr<ltproto::peer2peer::CaptureVideoFrame> DxgiVideoCapturer::capture_one_frame() {
    FRAME_DATA frame;
    bool timeout = false;
    auto hr = impl_->GetFrame(&frame, &timeout);
    if (hr == DUPL_RETURN::DUPL_RETURN_SUCCESS && !timeout) {
        std::string name = share_texture(frame.Frame);
        if (name.empty()) {
            return nullptr;
        }
        impl_->DoneWithFrame();
        auto capture_frame = std::make_shared<ltproto::peer2peer::CaptureVideoFrame>();
        capture_frame->set_name(name);
        capture_frame->set_capture_timestamp_us(ltlib::steady_now_us());
        return capture_frame;
    }
    return nullptr;
}

std::string DxgiVideoCapturer::share_texture(ID3D11Texture2D* texture) {
    std::string name;
    if (texture_pool_.empty()) {
        D3D11_TEXTURE2D_DESC desc;
        texture->GetDesc(&desc);
        desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
        desc.MiscFlags =
            D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX | D3D11_RESOURCE_MISC_SHARED_NTHANDLE;
        const size_t kDefaultPoolSize = 2;
        for (size_t i = 0; i < kDefaultPoolSize; i++) {
            ID3D11Texture2D* texture = NULL;
            auto hr = d3d11_dev_->CreateTexture2D(&desc, NULL, &texture);
            if (FAILED(hr)) {
                LOGF(WARNING, "failed to create shared texture, hr:0x%08x", hr);
                continue;
            }
            texture_pool_.push_back(texture);
            shared_handles_.push_back(0);
        }
        if (texture_pool_.empty()) {
            return name;
        }
    }
    IDXGIKeyedMutex* mutex = NULL;
    texture_pool_[index_]->QueryInterface(__uuidof(IDXGIKeyedMutex), (void**)&mutex);
    auto hr = mutex->AcquireSync(0, 0);
    if (FAILED(hr)) {
        mutex->Release();
        LOGF(WARNING, "drop frame, hr:0x%08x", hr);
        return name;
    }
    d3d11_ctx_->CopyResource(texture_pool_[index_], texture);

    IDXGIResource1* resource = NULL;
    hr = texture_pool_[index_]->QueryInterface(__uuidof(IDXGIResource1), (void**)&resource);
    if (FAILED(hr)) {
        LOGF(WARNING, "failed to get resource, hr:0x%08x", hr);
        mutex->ReleaseSync(1);
        mutex->Release();
        return name;
    }
    const std::string kSharedName = "Global\\lanthing_dxgi_sharedTexture_";
    name = kSharedName + std::to_string(index_);
    HANDLE handle = (HANDLE)shared_handles_[index_];
    if (handle == 0) {
        std::wstring w_name = ltlib::utf8_to_utf16(name);
        hr = resource->CreateSharedHandle(NULL, DXGI_SHARED_RESOURCE_READ, w_name.c_str(), &handle);
        if (FAILED(hr)) {
            LOGF(WARNING, "failed to create shared handle, hr:0x%08x", hr);
            mutex->ReleaseSync(1);
            mutex->Release();
            resource->Release();
            return name;
        }
        LOGF(WARNING, "handle: 0x%08x", handle);
        shared_handles_[index_] = (uint64_t)handle;
    }
    // FIXME: 如果没有人打开过handle, 会持续failed
    mutex->ReleaseSync(1);
    mutex->Release();
    resource->Release();
    index_ = (index_ + 1) % texture_pool_.size();
    return name;
}

// void DxgiVideoCapturer::done_with_frame()
//{
//     // 在shared_texture()已经copy过一次，马上救DoneWithFrame()了，这里不应该再调用
//     //impl_->DoneWithFrame();
// }

void DxgiVideoCapturer::wait_for_vblank() {
    impl_->WaitForVBlank();
}

} // namespace lt