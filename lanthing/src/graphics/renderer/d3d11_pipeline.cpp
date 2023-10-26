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

#include "d3d11_pipeline.h"

#include <cassert>

#include <d3dcompiler.h>
#include <dwmapi.h>

#include <ltlib/logging.h>

using namespace Microsoft::WRL;

#define _ALIGN(x, a) (((x) + (a)-1) & ~((a)-1))

namespace {

const std::string kVertexShader = R"(
struct VSIn
{
    float2 pos : POSITION;
    float2 tex : TEXCOORD0;
};

struct VSOut
{
    float2 tex : TEXCOORD0;
    float4 pos : SV_POSITION;
};

VSOut main_VS(VSIn vsin)
{
    VSOut vsout;
    vsout.pos = float4(vsin.pos, 0.0, 1.0);
    vsout.tex = vsin.tex;
    return vsout;
}
)";

const std::string kVideoPixelShader = R"(
// https://zhuanlan.zhihu.com/p/493035194
Texture2D<float> yChannel : register(t0);
Texture2D<float2> uvChannel : register(t1);

SamplerState splr;

cbuffer ColorMatrix : register(b0)
{
    float4x4 colorMatrix;
};

struct PSIn
{
    float2 tex : TEXCOORD0;
    float4 pos : SV_POSITION;
};

float4 main_PS(PSIn psin) : SV_TARGET
{
    float y = yChannel.Sample(splr, psin.tex);
    float2 uv = uvChannel.Sample(splr, psin.tex);
    float4 rgb = mul(float4(y, uv.x, uv.y, 1.0), colorMatrix);
    return rgb;
}
)";

const std::string kCursorPixelShader = R"(
Texture2D<float4> cursorTexture : t0;
SamplerState splr;

float4 main_PS(float2 tex : TEXCOORD) : SV_TARGET
{
    float4 color = cursorTexture.Sample(splr, tex);
    return color;
}
)";

struct Color {
    uint8_t B;
    uint8_t G;
    uint8_t R;
    uint8_t A;
};
static_assert(sizeof(Color) == 4);

struct Vertex {
    float x;
    float y;
    float u;
    float v;
    static const D3D11_INPUT_ELEMENT_DESC input_desc[2];
};

const D3D11_INPUT_ELEMENT_DESC Vertex::input_desc[2] = {
    {"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
    {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 8, D3D11_INPUT_PER_VERTEX_DATA, 0},
};

void mapTextureToFile(ID3D11Device* d3d11_dev, ID3D11DeviceContext* d3d11_context,
                      ID3D11Texture2D* texture, size_t index) {
    static ID3D11Texture2D* cpu_texture = NULL;
    static size_t width = 0;
    static size_t height = 0;
    if (!cpu_texture) {
        D3D11_TEXTURE2D_DESC desc1{};
        texture->GetDesc(&desc1);
        D3D11_TEXTURE2D_DESC desc2{};
        LOG(INFO) << "DESC width:" << desc1.Width << ", height:" << desc1.Height;
        width = desc1.Width;
        height = desc1.Height;
        desc2.Width = desc1.Width;
        desc2.Height = desc1.Height;
        desc2.MipLevels = 1;
        desc2.ArraySize = 1;
        desc2.Format = DXGI_FORMAT_NV12;
        desc2.SampleDesc.Count = 1;
        desc2.BindFlags = 0;
        desc2.MiscFlags = 0;
        // Create CPU access texture
        desc2.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
        desc2.Usage = D3D11_USAGE_STAGING;
        if (d3d11_dev->CreateTexture2D(&desc2, NULL, &cpu_texture) != S_OK) {
            return;
        }
    }
    d3d11_context->CopySubresourceRegion(cpu_texture, 0, 0, 0, 0, texture, static_cast<UINT>(index),
                                         nullptr);
    static size_t kSize = width * height * 3 / 2;
    static std::FILE* file = std::fopen("decoded.nv12", "wb");
    static uint8_t* buffer = new uint8_t[kSize];
    D3D11_MAPPED_SUBRESOURCE resource;
    UINT subresource = D3D11CalcSubresource(0, 0, 0);
    auto ret = d3d11_context->Map(cpu_texture, subresource, D3D11_MAP_READ_WRITE, 0, &resource);
    if (!SUCCEEDED(ret)) {
        LOGF(ERR, "fail to map texture");
        return;
    }
    memcpy(buffer, resource.pData, kSize);
    d3d11_context->Unmap(cpu_texture, subresource);

    std::fwrite(buffer, kSize, 1, file);
    std::fflush(file);
}

} // namespace

namespace lt {

D3D11Pipeline::D3D11Pipeline(const Params& params)
    : hwnd_{params.hwnd}
    , luid_{params.luid}
    , video_width_{params.widht}
    , video_height_{params.height}
    , align_{params.align} {
    DwmEnableMMCSS(TRUE);
}

D3D11Pipeline::~D3D11Pipeline() {
    if (waitable_obj_) {
        CloseHandle(waitable_obj_);
    }
}

bool D3D11Pipeline::bindTextures(const std::vector<void*>& _textures) {
    std::vector<ID3D11Texture2D*> textures;
    textures.resize(_textures.size());
    for (size_t i = 0; i < textures.size(); i++) {
        textures[i] = reinterpret_cast<ID3D11Texture2D*>(_textures[i]);
    }
    return initShaderResources(textures);
}

VideoRenderer::RenderResult D3D11Pipeline::render(int64_t frame) {
    // 1. 检查是否需要重置渲染目标
    // 2. 设置渲染目标
    // 3. 渲染视频
    // 4. 渲染光标
    RenderResult result = tryResetSwapChain();
    if (result == RenderResult::Failed) {
        return result;
    }
    // size_t index = static_cast<size_t>(frame);
    // mapTextureToFile(d3d11_dev_.Get(), d3d11_ctx_.Get(),
    //                  (ID3D11Texture2D*)shader_views_[index].texture, index);
    // std::lock_guard<std::mutex> lock(pipeline_mtx_);
    const float clear[4] = {0.f, 0.f, 0.f, 0.f};
    d3d11_ctx_->ClearRenderTargetView(render_view_.Get(), clear);
    d3d11_ctx_->OMSetRenderTargets(1, render_view_.GetAddressOf(), nullptr);
    RenderResult video_result = renderVideo(frame);
    if (video_result == RenderResult::Failed) {
        return result;
    }
    RenderResult cursor_result = renderCursor();
    if (cursor_result == RenderResult::Failed) {
        return result;
    }
    if (result == RenderResult::Reset || video_result == RenderResult::Reset ||
        cursor_result == RenderResult::Reset) {
        return RenderResult::Reset;
    }
    return RenderResult::Success;
}

void D3D11Pipeline::updateCursor(int32_t cursor_id, float x, float y, bool visible) {
    cursor_info_ = CursorInfo{cursor_id, x, y, visible};
}

void D3D11Pipeline::switchMouseMode(bool absolute) {
    absolute_mouse_ = absolute;
}

bool D3D11Pipeline::present() {
    auto hr = swap_chain_->Present(0, 0);
    pipeline_ready_ = false;
    if (FAILED(hr)) {
        LOGF(ERR, "failed to call presenter, hr:0x%08x", hr);
        return false;
    }
    return true;
}

void D3D11Pipeline::resetRenderTarget() {
    reset_ = true;
}

VideoRenderer::RenderResult D3D11Pipeline::tryResetSwapChain() {
    if (reset_.exchange(false)) {
        RECT rect;
        if (!GetClientRect(hwnd_, &rect)) {
            LOG(ERR) << "GetClientRect failed";
            return RenderResult::Failed;
        }
        display_width_ = rect.right - rect.left;
        display_height_ = rect.bottom - rect.top;
        render_view_ = nullptr;
        HRESULT hr = swap_chain_->ResizeBuffers(
            0, static_cast<UINT>(display_width_), static_cast<UINT>(display_height_),
            DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT);
        if (FAILED(hr)) {
            LOGF(ERR, "SwapChain resize buffers failed %#x", hr);
            return RenderResult::Failed;
        }
        if (waitable_obj_) {
            CloseHandle(waitable_obj_);
        }
        swap_chain_->SetMaximumFrameLatency(1);
        waitable_obj_ = swap_chain_->GetFrameLatencyWaitableObject();
        if (!waitable_obj_) {
            LOG(ERR) << "SwapChain GetFrameLatencyWaitableObject failed";
            return RenderResult::Failed;
        }
        ComPtr<ID3D11Resource> back_buffer;
        hr =
            swap_chain_->GetBuffer(0, __uuidof(ID3D11Resource), (void**)back_buffer.GetAddressOf());
        if (FAILED(hr)) {
            LOGF(ERR, "IDXGISwapChain::GetBuffer failed: %#x", hr);
            return RenderResult::Failed;
        }
        hr = d3d11_dev_->CreateRenderTargetView(back_buffer.Get(), nullptr, &render_view_);
        if (FAILED(hr)) {
            LOGF(ERR, "ID3D11Device::CreateRenderTargetView failed: %#x", hr);
            return RenderResult::Failed;
        }
        setupRSStage();
        return RenderResult::Reset;
    }
    return RenderResult::Success;
}

D3D11Pipeline::RenderResult D3D11Pipeline::renderVideo(int64_t frame) {
    d3d11_ctx_->VSSetShader(video_vertex_shader_.Get(), nullptr, 0);
    d3d11_ctx_->IASetInputLayout(video_input_layout_.Get());
    UINT stride = sizeof(Vertex);
    UINT offset = 0;
    ID3D11Buffer* vbarray[] = {video_vertex_buffer_.Get()};
    d3d11_ctx_->IASetVertexBuffers(0, 1, vbarray, &stride, &offset);
    d3d11_ctx_->IASetIndexBuffer(video_index_buffer_.Get(), DXGI_FORMAT_R32_UINT, 0);
    d3d11_ctx_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    d3d11_ctx_->PSSetShader(video_pixel_shader_.Get(), nullptr, 0);
    ID3D11Buffer* cbarray[] = {video_pixel_buffer_.Get()};
    d3d11_ctx_->PSSetConstantBuffers(0, 1, cbarray);
    d3d11_ctx_->PSSetSamplers(0, 1, video_sampler_.GetAddressOf());
    if (frame >= static_cast<int64_t>(video_shader_views_.size())) {
        LOG(ERR) << "Can not find shader view for texutre " << frame;
        return RenderResult::Failed;
    }
    size_t index = static_cast<size_t>(frame);
    ID3D11ShaderResourceView* const shader_views[2] = {video_shader_views_[index].y.Get(),
                                                       video_shader_views_[index].uv.Get()};
    d3d11_ctx_->PSSetShaderResources(0, 2, shader_views);
    d3d11_ctx_->DrawIndexed(6, 0, 0);
    return RenderResult::Success;
}

D3D11Pipeline::RenderResult D3D11Pipeline::renderCursor() {
    CursorInfo c = cursor_info_;
    if (absolute_mouse_ || !c.visible) {
        return RenderResult::Success;
    }
    auto iter = cursors_.find(c.id);
    if (iter == cursors_.end()) {
        iter = cursors_.find(0);
    }
    const float widht = 1.0f * iter->second.width / display_width_;
    const float height = 1.0f * iter->second.height / display_height_;
    Vertex verts[] = {{c.x - 0.5f, c.y - 0.5f, 0.0f, 0.0f},
                      {c.x - 0.5f + widht, c.y - 0.5f, 1.0f, 0.0f},
                      {c.x - 0.5f + widht, c.y - 0.5f - height, 1.0f, 1.0f},
                      {c.x - 0.5f, c.y - 0.5f - height, 0.0f, 1.0f}};
    D3D11_BUFFER_DESC vb_desc = {};
    vb_desc.ByteWidth = sizeof(verts);
    vb_desc.Usage = D3D11_USAGE_IMMUTABLE;
    vb_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vb_desc.CPUAccessFlags = 0;
    vb_desc.MiscFlags = 0;
    vb_desc.StructureByteStride = sizeof(Vertex);

    D3D11_SUBRESOURCE_DATA vb_data = {};
    vb_data.pSysMem = verts;
    cursor_vertex_buffer_ = nullptr;
    HRESULT hr = d3d11_dev_->CreateBuffer(&vb_desc, &vb_data, cursor_vertex_buffer_.GetAddressOf());
    if (FAILED(hr)) {
        LOGF(WARNING, "Failed to create vertext buffer, hr:0x%08x", hr);
        return RenderResult::Failed;
    }
    d3d11_ctx_->VSSetShader(video_vertex_shader_.Get(), nullptr, 0);
    d3d11_ctx_->IASetInputLayout(video_input_layout_.Get());
    UINT stride = sizeof(Vertex);
    UINT offset = 0;
    ID3D11Buffer* vbarray[] = {cursor_vertex_buffer_.Get()};
    d3d11_ctx_->IASetVertexBuffers(0, 1, vbarray, &stride, &offset);
    d3d11_ctx_->IASetIndexBuffer(video_index_buffer_.Get(), DXGI_FORMAT_R32_UINT, 0);
    d3d11_ctx_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    d3d11_ctx_->PSSetShader(cursor_pixel_shader_.Get(), nullptr, 0);
    d3d11_ctx_->PSSetSamplers(0, 1, cursor_sampler_.GetAddressOf());

    ID3D11ShaderResourceView* const shader_views[1] = {iter->second.view.Get()};
    d3d11_ctx_->PSSetShaderResources(0, 1, shader_views);
    d3d11_ctx_->DrawIndexed(6, 0, 0);
    return RenderResult::Success;
}

bool D3D11Pipeline::waitForPipeline(int64_t max_wait_ms) {
    if (!pipeline_ready_) {
        DWORD result = WaitForSingleObjectEx(waitable_obj_, static_cast<DWORD>(max_wait_ms), false);
        if (result == WAIT_OBJECT_0) {
            pipeline_ready_ = true;
            return true;
        }
        return false;
    }
    else {
        return true;
    }
}

void* D3D11Pipeline::hwDevice() {
    return d3d11_dev_.Get();
}

void* D3D11Pipeline::hwContext() {
    return d3d11_ctx_.Get();
}

uint32_t D3D11Pipeline::displayWidth() {
    return display_width_;
}

uint32_t D3D11Pipeline::displayHeight() {
    return display_height_;
}

bool D3D11Pipeline::init() {
    DWM_TIMING_INFO info;
    info.cbSize = sizeof(DWM_TIMING_INFO);
    HRESULT hr = DwmGetCompositionTimingInfo(nullptr, &info);

    if (FAILED(hr) || info.rateRefresh.uiDenominator == 0) {
        LOGF(ERR, "Failed to get dwm composition timing info, hr:0x%08x", hr);
        return false;
    }
    refresh_rate_ = info.rateRefresh.uiNumerator / info.rateRefresh.uiDenominator;

    if (!createD3D()) {
        return false;
    }
    if (!setupRenderPipeline()) {
        return false;
    }
    if (!createCursors()) {
        return false;
    }
    return true;
}

bool D3D11Pipeline::createD3D() {
    HRESULT hr = CreateDXGIFactory(__uuidof(IDXGIFactory3), (void**)&dxgi_factory_);
    if (FAILED(hr)) {
        LOGF(ERR, "Failed to create dxgi factory, err:%08lx", hr);
        return false;
    }

    ComPtr<IDXGIAdapter1> adapter;
    for (UINT i = 0;; i++) {
        DXGI_ADAPTER_DESC1 desc;
        hr = dxgi_factory_->EnumAdapters1(i, adapter.GetAddressOf());
        if (hr == DXGI_ERROR_NOT_FOUND) {
            break;
        }
        hr = adapter->GetDesc1(&desc);
        if (FAILED(hr)) {
            continue;
        }
        if (luid_ ==
            (((uint64_t)(desc.AdapterLuid.HighPart) << 32) + (uint64_t)desc.AdapterLuid.LowPart)) {
            break;
        }
    }
    if (adapter == NULL) {
        dxgi_factory_->EnumAdapters1(0, &adapter);
    }
    UINT flag = D3D11_CREATE_DEVICE_VIDEO_SUPPORT;
#ifdef _DEBUG
    flag |= D3D11_CREATE_DEVICE_DEBUG;
#endif
    hr = D3D11CreateDevice(adapter.Get(), D3D_DRIVER_TYPE_UNKNOWN, nullptr, flag, nullptr, 0,
                           D3D11_SDK_VERSION, d3d11_dev_.GetAddressOf(), nullptr,
                           d3d11_ctx_.GetAddressOf());
    if (FAILED(hr)) {
        LOGF(ERR, "Failed to create d3d11 device, err:%08lx", hr);
        return false;
    }
    ComPtr<ID3D10Multithread> d3d10_mt;
    hr = d3d11_dev_.As(&d3d10_mt);
    if (FAILED(hr)) {
        LOGF(ERR, "Cast to ID3D10Multithread failed: %#x", hr);
        return false;
    }
    d3d10_mt->SetMultithreadProtected(TRUE);
    return true;
}

bool D3D11Pipeline::setupRenderPipeline() {

    if (!setupRenderTarget() || !setupIAAndVSStage() || !setupRSStage() || !setupPSStage() ||
        !setupOMStage()) {
        return false;
    }
    LOGF(INFO, "d3d11 %u:%u, %u:%u", display_width_, display_height_, video_width_, video_height_);

    return true;
}

bool D3D11Pipeline::setupRenderTarget() {
    RECT rect;
    if (!GetClientRect(hwnd_, &rect)) {
        LOG(ERR) << "GetClientRect failed";
        return false;
    }
    display_width_ = rect.right - rect.left;
    display_height_ = rect.bottom - rect.top;
    DXGI_SWAP_CHAIN_DESC1 swap_chain_desc = {};
    swap_chain_desc.Stereo = FALSE;
    swap_chain_desc.SampleDesc.Count = 1;
    swap_chain_desc.SampleDesc.Quality = 0;
    swap_chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swap_chain_desc.Scaling = DXGI_SCALING_STRETCH;
    // DISCARD 和 DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL模式如何选择?
    swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swap_chain_desc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
    swap_chain_desc.Width = display_width_;
    swap_chain_desc.Height = display_height_;
    swap_chain_desc.BufferCount = 2;
    swap_chain_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swap_chain_desc.Flags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;
    ComPtr<IDXGISwapChain1> swap_chain;
    auto hr = dxgi_factory_->CreateSwapChainForHwnd(d3d11_dev_.Get(), hwnd_, &swap_chain_desc,
                                                    nullptr, nullptr, swap_chain.GetAddressOf());
    if (FAILED(hr)) {
        LOGF(INFO, "CreateSwapChainForHwnd failed: %#x", hr);
        return false;
    }
    hr = swap_chain->QueryInterface(__uuidof(IDXGISwapChain2), (void**)swap_chain_.GetAddressOf());
    if (FAILED(hr)) {
        LOGF(ERR, "IDXGISwapChain::QueryInterface(IDXGISwapChain4) failed: %#x ", hr);
        return false;
    }
    hr = dxgi_factory_->MakeWindowAssociation(hwnd_, DXGI_MWA_NO_WINDOW_CHANGES);
    if (FAILED(hr)) {
        LOGF(ERR, "IDXGIFactory::MakeWindowAssociation() failed: %#x", hr);
        return false;
    }
    swap_chain_->SetMaximumFrameLatency(1);
    waitable_obj_ = swap_chain_->GetFrameLatencyWaitableObject();
    if (!waitable_obj_) {
        return false;
    }

    ComPtr<ID3D11Resource> back_buffer;
    hr = swap_chain_->GetBuffer(0, __uuidof(ID3D11Resource), (void**)back_buffer.GetAddressOf());
    if (FAILED(hr)) {
        LOGF(ERR, "IDXGISwapChain::GetBuffer failed: %#x", hr);
        return false;
    }
    hr = d3d11_dev_->CreateRenderTargetView(back_buffer.Get(), nullptr, &render_view_);
    if (FAILED(hr)) {
        LOGF(ERR, "ID3D11Device::CreateRenderTargetView failed: %#x", hr);
        return false;
    }
    return true;
}

bool D3D11Pipeline::setupIAAndVSStage() {
    ComPtr<ID3DBlob> blob;
    UINT flags1 = 0;
#ifdef _DEBUG
    flags1 |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
    auto hr = D3DCompile(kVertexShader.c_str(), kVertexShader.size(), NULL, NULL, NULL, "main_VS",
                         "vs_5_0", flags1, 0, blob.GetAddressOf(), NULL);
    if (FAILED(hr)) {
        LOGF(WARNING, "Failed to compile vertex shader, hr:0x%08x", hr);
        return false;
    }
    hr = d3d11_dev_->CreateVertexShader(blob->GetBufferPointer(), blob->GetBufferSize(), NULL,
                                        video_vertex_shader_.GetAddressOf());
    if (FAILED(hr)) {
        LOGF(WARNING, "Failed to create vertex shader, hr:0x%08x", hr);
        return false;
    }

    hr = d3d11_dev_->CreateInputLayout(Vertex::input_desc, ARRAYSIZE(Vertex::input_desc),
                                       blob->GetBufferPointer(), blob->GetBufferSize(),
                                       video_input_layout_.GetAddressOf());
    if (FAILED(hr)) {
        LOGF(WARNING, "Failed to create input layout: %#x", hr);
        return false;
    }
    float u = (float)video_width_ / _ALIGN(video_width_, align_);
    float v = (float)video_height_ / _ALIGN(video_height_, align_);
    Vertex verts[] = {{-1.0f, 1.0f, 0.0f, 0.0f},
                      {1.0f, 1.0f, u, 0.0f},
                      {1.0f, -1.0f, u, v},
                      {-1.0f, -1.0f, 0.0f, v}};
    D3D11_BUFFER_DESC vb_desc = {};
    vb_desc.ByteWidth = sizeof(verts);
    vb_desc.Usage = D3D11_USAGE_IMMUTABLE;
    vb_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vb_desc.CPUAccessFlags = 0;
    vb_desc.MiscFlags = 0;
    vb_desc.StructureByteStride = sizeof(Vertex);

    D3D11_SUBRESOURCE_DATA vb_data = {};
    vb_data.pSysMem = verts;

    hr = d3d11_dev_->CreateBuffer(&vb_desc, &vb_data, video_vertex_buffer_.GetAddressOf());
    if (FAILED(hr)) {
        LOGF(WARNING, "Failed to create vertext buffer, hr:0x%08x", hr);
        return false;
    }

    // 索引
    const uint32_t indexes[] = {0, 1, 2, 0, 2, 3};
    D3D11_BUFFER_DESC index_buffer_desc = {};
    index_buffer_desc.ByteWidth = sizeof(indexes);
    index_buffer_desc.Usage = D3D11_USAGE_IMMUTABLE;
    index_buffer_desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    index_buffer_desc.CPUAccessFlags = 0;
    index_buffer_desc.MiscFlags = 0;
    index_buffer_desc.StructureByteStride = sizeof(uint32_t);

    D3D11_SUBRESOURCE_DATA index_buffer_data = {};
    index_buffer_data.pSysMem = indexes;
    index_buffer_data.SysMemPitch = sizeof(uint32_t);

    hr = d3d11_dev_->CreateBuffer(&index_buffer_desc, &index_buffer_data,
                                  video_index_buffer_.GetAddressOf());
    if (FAILED(hr)) {
        LOGF(WARNING, "Failed to create index buffer, 0x%08x", hr);
        return false;
    }
    return true;
}

bool D3D11Pipeline::setupRSStage() {
    D3D11_VIEWPORT viewport{};
    viewport.TopLeftX = 0;
    viewport.TopLeftY = 0;
    viewport.Width = (float)display_width_;
    viewport.Height = (float)display_height_;
    viewport.MinDepth = 0;
    viewport.MaxDepth = 1;
    d3d11_ctx_->RSSetViewports(1, &viewport);

    return true;
}

bool D3D11Pipeline::setupPSStage() {
    ComPtr<ID3DBlob> blob = nullptr;
    UINT flags1 = 0;
#ifdef _DEBUG
    flags1 |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
    auto hr = D3DCompile(kVideoPixelShader.c_str(), kVideoPixelShader.size(), NULL, NULL, NULL,
                         "main_PS", "ps_5_0", flags1, 0, blob.GetAddressOf(), NULL);
    if (FAILED(hr)) {
        LOGF(WARNING, "Failed to compile pixel shader, hr:0x%08x", hr);
        return false;
    }
    hr = d3d11_dev_->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), NULL,
                                       video_pixel_shader_.GetAddressOf());
    if (FAILED(hr)) {
        LOGF(WARNING, "Failed to create pixel shader, hr:0x%08x", hr);
        return false;
    }

    D3D11_BUFFER_DESC const_desc = {};
    const_desc.ByteWidth = sizeof(ColorMatrix);
    const_desc.Usage = D3D11_USAGE_IMMUTABLE;
    const_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    const_desc.CPUAccessFlags = 0;
    const_desc.MiscFlags = 0;

    ColorMatrix color_matrix = getColorMatrix();
    D3D11_SUBRESOURCE_DATA const_data = {};
    const_data.pSysMem = &color_matrix;

    hr = d3d11_dev_->CreateBuffer(&const_desc, &const_data, video_pixel_buffer_.GetAddressOf());
    if (FAILED(hr)) {
        LOGF(WARNING, "Failed to create const buffer, hr:0x%08x", hr);
        return false;
    }

    D3D11_SAMPLER_DESC sample_desc = {};
    sample_desc.MipLODBias = 0.0f;
    sample_desc.MaxAnisotropy = 1;
    sample_desc.MinLOD = 0.0f;
    sample_desc.MaxLOD = D3D11_FLOAT32_MAX;
    sample_desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    sample_desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sample_desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sample_desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sample_desc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;

    hr = d3d11_dev_->CreateSamplerState(&sample_desc, video_sampler_.GetAddressOf());
    if (FAILED(hr)) {
        LOGF(WARNING, "Failed to create sample state, hr:0x%08x", hr);
        return false;
    }
    return true;
}

bool D3D11Pipeline::setupOMStage() {
    D3D11_BLEND_DESC desc = {};
    desc.AlphaToCoverageEnable = FALSE;
    desc.IndependentBlendEnable = FALSE;
    desc.RenderTarget[0].BlendEnable = TRUE;
    desc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    desc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
    desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

    ComPtr<ID3D11BlendState> bs;
    auto hr = d3d11_dev_->CreateBlendState(&desc, bs.GetAddressOf());
    if (FAILED(hr)) {
        LOGF(WARNING, "Failed to create blend state, hr:0x%08x", hr);
        return false;
    }
    d3d11_ctx_->OMSetBlendState(bs.Get(), nullptr, 0xffffffff);
    return true;
}

bool D3D11Pipeline::initShaderResources(const std::vector<ID3D11Texture2D*>& textures) {
    video_shader_views_.resize(textures.size());
    D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
    srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
    srv_desc.Texture2DArray.MostDetailedMip = 0;
    srv_desc.Texture2DArray.MipLevels = 1;
    srv_desc.Texture2DArray.ArraySize = 1;
    for (size_t i = 0; i < video_shader_views_.size(); i++) {
        video_shader_views_[i].texture = textures[i];
        srv_desc.Format = DXGI_FORMAT_R8_UNORM;
        srv_desc.Texture2DArray.FirstArraySlice = static_cast<UINT>(i);
        auto hr = d3d11_dev_->CreateShaderResourceView(textures[i], &srv_desc,
                                                       video_shader_views_[i].y.GetAddressOf());
        if (FAILED(hr)) {
            LOGF(WARNING, "ID3D11Device::CreateShaderResourceView() failed: 0x%08x", hr);
            return false;
        }
        srv_desc.Format = DXGI_FORMAT_R8G8_UNORM;
        hr = d3d11_dev_->CreateShaderResourceView(textures[i], &srv_desc,
                                                  video_shader_views_[i].uv.GetAddressOf());
        if (FAILED(hr)) {
            LOGF(WARNING, "ID3D11Device::CreateShaderResourceView() failed: 0x%08x", hr);
            return false;
        }
    }
    return true;
}

bool D3D11Pipeline::createCursors() {
    std::vector<LPSTR> kCursors = {IDC_ARROW,    IDC_IBEAM,    IDC_WAIT,   IDC_CROSS,
                                   IDC_SIZENWSE, IDC_SIZENESW, IDC_SIZEWE, IDC_SIZENS,
                                   IDC_SIZEALL,  IDC_NO,       IDC_HAND};
    for (size_t i = 0; i < kCursors.size(); i++) {
        int32_t width = 0;
        int32_t height = 0;
        std::vector<uint8_t> data;
        if (!loadCursorAsBitmap(kCursors[i], width, height, data)) {
            if (i == 0) {
                // 普通箭头必须成功，作为兜底项
                return false;
            }
            continue;
        }
        if (!createCursorResourceFromBitmap(i, width, height, data)) {
            return false;
        }
    }
    if (!setupCursorD3DResources()) {
        return false;
    }
    return true;
}

bool D3D11Pipeline::loadCursorAsBitmap(char* name, int32_t& out_width, int32_t& out_height,
                                       std::vector<uint8_t>& out_data) {
    auto cursor = LoadCursorA(nullptr, name);
    if (cursor == nullptr) {
        LOG(ERR) << "LoadCursor failed: 0x" << std::hex << GetLastError();
        return false;
    }
    ICONINFO iconinfo{};
    BOOL ret = GetIconInfo(cursor, &iconinfo);
    if (ret != TRUE) {
        LOG(ERR) << "GetIconInfo failed: 0x" << std::hex << GetLastError();
        return false;
    }
    std::vector<uint8_t> color_data;
    std::vector<uint8_t> mask_data;
    int32_t color_width = 0;
    int32_t color_height = 0;
    int32_t color_bits_pixel = 0;
    int32_t mask_width = 0;
    int32_t mask_height = 0;
    int32_t mask_bits_pixel = 0;
    do {
        if (iconinfo.hbmMask) {
            BITMAP bmp{};
            GetObjectA(iconinfo.hbmMask, sizeof(BITMAP), &bmp);
            if (!bmp.bmWidthBytes || !bmp.bmHeight) {
                break;
            }

            mask_data.resize(bmp.bmWidthBytes * bmp.bmHeight);
            if (!GetBitmapBits(iconinfo.hbmMask, bmp.bmWidthBytes * bmp.bmHeight,
                               mask_data.data())) {
                break;
            }

            mask_width = bmp.bmWidth;
            mask_height = bmp.bmHeight;
            mask_bits_pixel = bmp.bmBitsPixel;
        }
        if (iconinfo.hbmColor) {
            BITMAP bmp{};
            GetObjectA(iconinfo.hbmColor, sizeof(BITMAP), &bmp);
            if (!bmp.bmWidthBytes || !bmp.bmHeight) {
                break;
            }

            color_data.resize(bmp.bmWidthBytes * bmp.bmHeight);
            if (!GetBitmapBits(iconinfo.hbmColor, bmp.bmWidthBytes * bmp.bmHeight,
                               color_data.data())) {
                break;
            }

            color_width = bmp.bmWidth;
            color_height = bmp.bmHeight;
            color_bits_pixel = bmp.bmBitsPixel;
        }
    } while (false);
    if (iconinfo.hbmColor) {
        DeleteObject(iconinfo.hbmColor);
    }
    if (iconinfo.hbmMask) {
        DeleteObject(iconinfo.hbmMask);
    }
    if (cursor) {
        DestroyCursor(cursor);
    }
    std::vector<Color> cursor_color;
    if (color_data.empty()) {
        if (!mask_data.empty() && mask_bits_pixel == 1) {
            std::vector<uint8_t> mask_data2(mask_data.cbegin(),
                                            mask_data.cbegin() + mask_data.size() / 2);
            std::vector<uint8_t> color_data2(mask_data.cbegin() + mask_data.size() / 2,
                                             mask_data.cend());
            cursor_color.resize(mask_width * mask_height / 2);
            for (int32_t i = 0; i < mask_height / 2; ++i) {
                for (int32_t j = 0; j < mask_width; ++j) {
                    auto index = i * mask_data.size() / mask_height + j / 8;
                    auto pos = j % 8;
                    auto bit_mask = (mask_data2[index] & (0b10000000 >> pos)) ? 1 : 0;
                    auto bit_color = (color_data2[index] & (0b10000000 >> pos)) ? 1 : 0;
                    cursor_color[i * mask_width + j].B = bit_color ? 255 : 0;
                    cursor_color[i * mask_width + j].G = bit_color ? 255 : 0;
                    cursor_color[i * mask_width + j].R = bit_color ? 255 : 0;
                    cursor_color[i * mask_width + j].A =
                        static_cast<uint8_t>(bit_mask ? bit_color * 255 : 255);
                }
            }
        }
    }
    else {
        cursor_color.resize(color_width * color_height);
        for (int32_t i = 0; i < color_height; ++i) {
            for (int32_t j = 0; j < color_width; ++j) {
                cursor_color[i * color_width + j].B = color_data[i * color_width * 4 + j * 4];
                cursor_color[i * color_width + j].G = color_data[i * color_width * 4 + j * 4 + 1];
                cursor_color[i * color_width + j].R = color_data[i * color_width * 4 + j * 4 + 2];
                cursor_color[i * mask_width + j].A = color_data[i * color_width * 4 + j * 4 + 3];
            }
        }
    }
    out_width = mask_width;
    out_height = mask_height;
    if (color_data.empty() && mask_bits_pixel == 1) {
        out_height = out_height / 2;
    }
    out_data.resize(cursor_color.size() * sizeof(Color));
    memcpy(out_data.data(), cursor_color.data(), out_data.size());
    return true;
}

bool D3D11Pipeline::createCursorResourceFromBitmap(size_t id, int32_t width, int32_t height,
                                                   const std::vector<uint8_t>& data) {
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    desc.Usage = D3D11_USAGE_DEFAULT;
    D3D11_SUBRESOURCE_DATA subresource{};
    subresource.pSysMem = data.data();
    subresource.SysMemPitch = sizeof(Color) * width;
    subresource.SysMemSlicePitch = 0;
    ComPtr<ID3D11Texture2D> texture;
    HRESULT hr = d3d11_dev_->CreateTexture2D(&desc, &subresource, texture.GetAddressOf());
    if (hr != S_OK) {
        LOGF(ERR, "CreateTexture2D failed with %#x", hr);
        return false;
    }
    D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
    srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srv_desc.Texture2D.MostDetailedMip = 0;
    srv_desc.Texture2D.MipLevels = 1;
    srv_desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    ComPtr<ID3D11ShaderResourceView> sv;
    hr = d3d11_dev_->CreateShaderResourceView(texture.Get(), &srv_desc, sv.GetAddressOf());
    if (hr != S_OK) {
        LOGF(ERR, "CreateShaderResourceView failed with %#x", hr);
        return false;
    }
    CursorRes cr{};
    cr.texture = texture;
    cr.view = sv;
    cr.width = width;
    cr.height = height;
    cursors_[id] = cr;
    LOG(INFO) << "Create D3D11 Resource for cursor " << id << " success";
    return true;
}

bool D3D11Pipeline::setupCursorD3DResources() {
    D3D11_SAMPLER_DESC samplerDesc = {};
    samplerDesc.Filter = D3D11_FILTER::D3D11_FILTER_ANISOTROPIC;
    samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.MaxAnisotropy = 16;
    HRESULT hr = d3d11_dev_->CreateSamplerState(&samplerDesc, &cursor_sampler_);
    if (hr != S_OK) {
        LOGF(ERR, "CreateSamplerState failed with %#x", hr);
        return false;
    }
    ComPtr<ID3DBlob> blob;
    UINT flags1 = 0;
#ifdef _DEBUG
    flags1 |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
    hr = D3DCompile(kCursorPixelShader.c_str(), kCursorPixelShader.size(), NULL, NULL, NULL,
                    "main_PS", "ps_5_0", flags1, 0, blob.GetAddressOf(), NULL);
    if (FAILED(hr)) {
        LOGF(WARNING, "Failed to compile vertex shader, hr:0x%08x", hr);
        return false;
    }
    hr = d3d11_dev_->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), NULL,
                                       cursor_pixel_shader_.GetAddressOf());
    if (FAILED(hr)) {
        LOGF(WARNING, "Failed to create pixel shader, hr:0x%08x", hr);
        return false;
    }
    return true;
}

const D3D11Pipeline::ColorMatrix& D3D11Pipeline::getColorMatrix() const {
    // TODO:
    // 颜色空间转换系数的选择，应该从编码的时候就确定，下策是从解码器中探知，下下策是乱选一个
    // https://zhuanlan.zhihu.com/p/493035194
    // clang-format off
    static const ColorMatrix kBT709 =
        ColorMatrix{{
                1.1643835616f, 0.0000000000f, 1.7927410714f, -0.9729450750f,
                1.1643835616f, -0.2132486143f, -0.5329093286f, 0.3014826655f,
                1.1643835616f, 2.1124017857f, 0.00000000000f, -1.1334022179f,
                0.0f, 0.0f, 0.0f, 1.0f}};
    // clang-format on
    return kBT709;
}

std::optional<D3D11Pipeline::ShaderView> D3D11Pipeline::getShaderView(void* texture) {
    for (auto& view : video_shader_views_) {
        if (view.texture == texture) {
            return view;
        }
    }
    return std::nullopt;
}

} // namespace lt