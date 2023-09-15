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
#include <graphics/renderer/video_renderer.h>

#include <cstdint>
#include <optional>
#include <vector>

#include <d3d11_1.h>
#include <dxgi1_3.h>
#include <wrl/client.h>

namespace lt {

class D3D11Pipeline : public VideoRenderer {
    struct ColorMatrix {
        float matrix[16];
    };
    struct ShaderView {
        void* texture = nullptr;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> y;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> uv;
    };

public:
    struct Params {
        HWND hwnd;
        uint64_t luid;
        uint32_t widht;
        uint32_t height;
        uint32_t align;
    };

public:
    D3D11Pipeline(const Params& params);
    ~D3D11Pipeline() override;
    bool init();
    bool bindTextures(const std::vector<void*>& textures) override;
    bool render(int64_t frame) override;
    bool present() override;
    bool waitForPipeline(int64_t max_wait_ms) override;
    void* hwDevice() override;
    void* hwContext() override;
    uint32_t displayWidth() override;
    uint32_t displayHeight() override;

private:
    bool createD3D();
    bool setupRenderPipeline();
    bool setupIAAndVSStage();
    bool setupRSStage();
    bool setupPSStage();
    bool setupOMStage();
    bool initShaderResources(const std::vector<ID3D11Texture2D*>& textures);
    const ColorMatrix& getColorMatrix() const;
    std::optional<ShaderView> getShaderView(void* texture);

private:
    HWND hwnd_;
    const uint64_t luid_;
    const uint32_t video_width_;
    const uint32_t video_height_;
    const uint32_t align_;
    int refresh_rate_ = 60;

    Microsoft::WRL::ComPtr<ID3D11Device> d3d11_dev_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> d3d11_ctx_ = nullptr;

    Microsoft::WRL::ComPtr<IDXGIFactory3> dxgi_factory_ = nullptr;
    Microsoft::WRL::ComPtr<IDXGISwapChain2> swap_chain_ = nullptr;
    HANDLE waitable_obj_ = NULL;
    bool pipeline_ready_ = false;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> render_view_ = nullptr;
    std::vector<ShaderView> shader_views_;

    uint32_t display_width_ = 0;
    uint32_t display_height_ = 0;
};

} // namespace lt