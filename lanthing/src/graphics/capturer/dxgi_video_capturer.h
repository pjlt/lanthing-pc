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

#pragma once
#include <wrl/client.h>

#include <atomic>
#include <optional>

#include <graphics/capturer/dxgi/duplication_manager.h>
#include <graphics/capturer/video_capturer.h>

namespace lt {

class DxgiVideoCapturer : public VideoCapturer {
    struct SharedTexture {
        std::string name;
        Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
        HANDLE handle = nullptr;
        std::atomic<bool> in_use{false};
    };

public:
    DxgiVideoCapturer();
    ~DxgiVideoCapturer() override;
    bool preInit() override;
    std::shared_ptr<ltproto::peer2peer::CaptureVideoFrame> captureOneFrame() override;
    void releaseFrame(const std::string& name);
    void waitForVblank() override;
    Backend backend() const override;
    int64_t luid() override;

private:
    bool initD3D11();
    std::string shareTexture(ID3D11Texture2D* texture);
    std::optional<size_t> getFreeSharedTexture();

private:
    std::unique_ptr<DUPLICATIONMANAGER> impl_;
    Microsoft::WRL::ComPtr<IDXGIFactory1> dxgi_factory_;
    Microsoft::WRL::ComPtr<ID3D11Device> d3d11_dev_;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> d3d11_ctx_;
    static const size_t kDefaultPoolSize = 2;
    std::vector<SharedTexture> texture_pool_;
    bool pool_inited_ = false;
    int64_t luid_;
};

} // namespace lt