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
    bool waitForPipeline(int64_t max_wait_ms) override;
    void* hwDevice() override;
    void* hwContext() override;

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