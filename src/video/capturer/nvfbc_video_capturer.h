/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2025 Zhennan Tu <zhennan.tu@gmail.com>
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
#include <d3d9.h>
#include <wrl/client.h>

#include <atomic>
#include <future>
#include <optional>

#include <NvFBC/nvFBC.h>
#include <NvFBC/nvFBCToDx9Vid.h>

#include <ltlib/system.h>

#include <video/capturer/nvfbc/NvFBCLibrary.h>
#include <video/capturer/video_capturer.h>

namespace lt {

namespace video {

class NvFBCVideoCapturer : public Capturer {
public:
    NvFBCVideoCapturer(ltlib::Monitor monitor);
    ~NvFBCVideoCapturer() override;
    bool init() override;
    bool start() override;
    std::optional<Capturer::Frame> capture() override;
    std::optional<CursorInfo> cursorInfo() override;
    void doneWithFrame() override;
    void waitForVBlank() override;
    Backend backend() const override;
    int64_t luid() override;
    void* device() override;
    void* deviceContext() override;
    uint32_t vendorID() override;
    bool defaultOutput() override;
    bool setCaptureFormat(CaptureFormat format) override;

private:
    bool initD3D9();
#if 0
    bool initD3D11();
    uint8_t* toI420(ID3D11Texture2D* frame);
    void saveCursorInfo(DXGI_OUTDUPL_FRAME_INFO* frame_info);
#endif

private:
    std::unique_ptr<NvFBCLibrary> impl_;
    Microsoft::WRL::ComPtr<IDirect3D9Ex> d3d9_ex_;
    Microsoft::WRL::ComPtr<IDirect3DDevice9Ex> d3d9_dev_;
    NvFBCToDx9Vid* nvfbc_dx9_ = nullptr;
    NVFBC_TODX9VID_OUT_BUF nvfbc_outbuf_{};
    Microsoft::WRL::ComPtr<IDirect3DSurface9> d3d9_surface_;
    D3DDISPLAYMODE display_mode_{};
#if 0
    Microsoft::WRL::ComPtr<IDXGIFactory1> dxgi_factory_;
    Microsoft::WRL::ComPtr<ID3D11Device> d3d11_dev_;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> d3d11_ctx_;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> stage_texture_;
#endif
    std::vector<uint8_t> mem_buff_;
    int64_t luid_ = 0;
    int32_t adapter_index_ = -1;
    ltlib::Monitor monitor_;
    CaptureFormat capture_foramt_ = CaptureFormat::D3D11_BGRA;
    std::optional<CursorInfo> cursor_info_;
};

} // namespace video

} // namespace lt