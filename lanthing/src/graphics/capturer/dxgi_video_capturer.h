#pragma once
#include <graphics/capturer/video_capturer.h>
#include <graphics/capturer/dxgi/duplication_manager.h>
#include <wrl/client.h>

namespace lt
{

class DxgiVideoCapturer : public VideoCapturer
{
public:
    DxgiVideoCapturer();
    ~DxgiVideoCapturer() override;
    bool pre_init() override;
    std::shared_ptr<ltproto::peer2peer::CaptureVideoFrame> capture_one_frame() override;
    //void done_with_frame() override;
    void wait_for_vblank() override;

private:
    bool init_d3d11();
    std::string share_texture(ID3D11Texture2D* texture);

private:
    std::unique_ptr<DUPLICATIONMANAGER> impl_;
    Microsoft::WRL::ComPtr<IDXGIFactory1> dxgi_factory_;
    Microsoft::WRL::ComPtr<ID3D11Device> d3d11_dev_;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> d3d11_ctx_;
    size_t index_ = 0;
    std::vector<ID3D11Texture2D*> texture_pool_;
    std::vector<uint64_t> shared_handles_;
};

} // namespace lt