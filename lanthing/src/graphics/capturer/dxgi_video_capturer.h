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
    bool pre_init() override;
    std::shared_ptr<ltproto::peer2peer::CaptureVideoFrame> capture_one_frame() override;
    void release_frame(const std::string& name);
    void wait_for_vblank() override;
    Backend backend() const override;
    int64_t luid() override;

private:
    bool init_d3d11();
    std::string share_texture(ID3D11Texture2D* texture);
    std::optional<size_t> get_free_shared_texture();

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