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
#include <future>
#include <optional>

#include <ltlib/system.h>

#include <video/capturer/dxgi/duplication_manager.h>
#include <video/capturer/video_capturer.h>

namespace lt {

namespace video {

class DxgiVideoCapturer : public Capturer {
public:
    DxgiVideoCapturer(ltlib::Monitor monitor);
    ~DxgiVideoCapturer() override;
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
    bool initD3D11();
    uint8_t* toI420(ID3D11Texture2D* frame);
    void saveCursorInfo(DXGI_OUTDUPL_FRAME_INFO* frame_info);

private:
    std::unique_ptr<DUPLICATIONMANAGER> impl_;
    Microsoft::WRL::ComPtr<IDXGIFactory1> dxgi_factory_;
    Microsoft::WRL::ComPtr<ID3D11Device> d3d11_dev_;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> d3d11_ctx_;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> stage_texture_;
    std::vector<uint8_t> mem_buff_;
    int64_t luid_ = 0;
    uint32_t vendor_id_ = 0;
    ltlib::Monitor monitor_;
    CaptureFormat capture_foramt_ = CaptureFormat::D3D11_BGRA;
    std::optional<CursorInfo> cursor_info_;
};

} // namespace video

} // namespace lt