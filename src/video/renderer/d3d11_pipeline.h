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
#include <video/renderer/video_renderer.h>

#include <atomic>
#include <cstdint>
#include <map>
#include <optional>
#include <vector>

#include <d3d11_1.h>
#include <dxgi1_3.h>
#include <wrl/client.h>

namespace lt {

namespace video {

class D3D11Pipeline : public Renderer {
    struct ColorMatrix {
        float matrix[16];
    };
    struct ShaderView {
        void* texture = nullptr;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> y;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> uv;
    };
    struct CursorRes {
        Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> view;
        int32_t width;
        int32_t height;
    };

public:
    struct Params {
        void* window;
        void* device;
        void* context;
        uint32_t widht;
        uint32_t height;
        uint32_t rotation;
        uint32_t align;
        bool stretch;
        bool absolute_mouse;
    };

public:
    D3D11Pipeline(const Params& params);
    ~D3D11Pipeline() override;
    bool init();
    bool bindTextures(const std::vector<void*>& textures) override;
    RenderResult render(int64_t frame) override;
    void updateCursor(const std::optional<lt::CursorInfo>& cursor_info) override;
    void switchMouseMode(bool absolute) override;
    void switchStretchMode(bool stretch) override;
    void resetRenderTarget() override;
    bool present() override;
    bool waitForPipeline(int64_t max_wait_ms) override;
    void* hwDevice() override;
    void* hwContext() override;
    uint32_t displayWidth() override;
    uint32_t displayHeight() override;
    bool setDecodedFormat(DecodedFormat format) override;

private:
    bool createD3D();
    bool setupRenderPipeline();
    bool setupRenderTarget();
    bool setupIAAndVSStage();
    bool setupRSStage();
    bool setupPSStage();
    bool setupOMStage();
    bool initShaderResources(const std::vector<ID3D11Texture2D*>& textures);
    bool createCursors();
    bool loadCursorAsBitmap(char* name, int32_t& width, int32_t& height,
                            std::vector<uint8_t>& data);
    bool createCursorResourceFromBitmap(size_t id, int32_t width, int32_t height,
                                        const std::vector<uint8_t>& data);
    bool calcVertexes();
    bool setupCursorD3DResources();
    const ColorMatrix& getColorMatrix() const;
    std::optional<ShaderView> getShaderView(void* texture);
    RenderResult tryResetSwapChain();
    RenderResult renderVideo(int64_t frame);
    RenderResult renderCursor();
    RenderResult renderPresetCursor(const lt::CursorInfo& info);
    RenderResult renderDataCursor(const lt::CursorInfo& info);
    auto createCursorTextures(const lt::CursorInfo& c)
        -> std::tuple<Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>,
                      Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>>;
    auto createCursorTexture(const uint8_t* data, uint32_t w, int32_t h)
        -> Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>;

private:
    HWND hwnd_;
    const uint32_t video_width_;
    const uint32_t video_height_;
    const uint32_t rotation_;
    const uint32_t align_;
    int refresh_rate_ = 60;
    DecodedFormat decoded_foramt_ = DecodedFormat::D3D11_NV12;

    Microsoft::WRL::ComPtr<ID3D11Device> d3d11_dev_;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> d3d11_ctx_;

    Microsoft::WRL::ComPtr<IDXGIFactory3> dxgi_factory_ = nullptr;
    Microsoft::WRL::ComPtr<IDXGISwapChain2> swap_chain_ = nullptr;
    HANDLE waitable_obj_ = NULL;
    bool pipeline_ready_ = false;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> stage_texture_;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> render_view_ = nullptr;
    std::vector<ShaderView> video_shader_views_;
    Microsoft::WRL::ComPtr<ID3D11VertexShader> video_vertex_shader_;
    Microsoft::WRL::ComPtr<ID3D11InputLayout> video_input_layout_;
    Microsoft::WRL::ComPtr<ID3D11Buffer> video_vertex_buffer_;
    Microsoft::WRL::ComPtr<ID3D11Buffer> video_index_buffer_;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> video_pixel_shader_;
    Microsoft::WRL::ComPtr<ID3D11Buffer> video_pixel_buffer_;
    Microsoft::WRL::ComPtr<ID3D11SamplerState> video_sampler_;

    std::map<size_t, CursorRes> cursors_;
    Microsoft::WRL::ComPtr<ID3D11VertexShader> cursor_vertex_shader_;
    Microsoft::WRL::ComPtr<ID3D11InputLayout> cursor_input_layout_;
    Microsoft::WRL::ComPtr<ID3D11Buffer> cursor_vertex_buffer_;
    Microsoft::WRL::ComPtr<ID3D11Buffer> cursor_index_buffer_;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> cursor_pixel_shader_;
    // Microsoft::WRL::ComPtr<ID3D11Buffer> cursor_pixel_buffer_;
    Microsoft::WRL::ComPtr<ID3D11SamplerState> cursor_sampler_;
    Microsoft::WRL::ComPtr<ID3D11BlendState> blend_cursor1_{};
    Microsoft::WRL::ComPtr<ID3D11BlendState> blend_cursor2_{};
    Microsoft::WRL::ComPtr<ID3D11BlendState> blend_screen_{};
    std::optional<lt::CursorInfo> cursor_info_;
    bool absolute_mouse_;
    bool stretch_;
    uint32_t display_width_ = 0;
    uint32_t display_height_ = 0;
    std::atomic<bool> reset_{false};
};

} // namespace video

} // namespace lt