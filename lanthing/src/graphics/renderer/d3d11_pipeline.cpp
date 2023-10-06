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

const std::string kPixelShader = R"(
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

bool D3D11Pipeline::render(int64_t frame) {
    tryResetSwapChain();
    if (frame >= static_cast<int64_t>(shader_views_.size())) {
        LOGF(ERR, "Can not find shader view for texutre %d", frame);
        return false;
    }
    size_t index = static_cast<size_t>(frame);
    // mapTextureToFile(d3d11_dev_.Get(), d3d11_ctx_.Get(),
    //                  (ID3D11Texture2D*)shader_views_[index].texture, index);
    //  std::lock_guard<std::mutex> lock(pipeline_mtx_);
    const float clear[4] = {0.f, 0.f, 0.f, 0.f};
    ID3D11ShaderResourceView* const shader_views[2] = {shader_views_[index].y.Get(),
                                                       shader_views_[index].uv.Get()};
    d3d11_ctx_->ClearRenderTargetView(render_view_.Get(), clear);
    d3d11_ctx_->OMSetRenderTargets(1, render_view_.GetAddressOf(), nullptr);
    d3d11_ctx_->PSSetShaderResources(0, 2, shader_views);
    d3d11_ctx_->DrawIndexed(6, 0, 0);
    return true;
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

bool D3D11Pipeline::tryResetSwapChain() {
    if (reset_.exchange(false)) {
        RECT rect;
        if (!GetClientRect(hwnd_, &rect)) {
            LOG(ERR) << "GetClientRect failed";
            return false;
        }
        display_width_ = rect.right - rect.left;
        display_height_ = rect.bottom - rect.top;
        render_view_ = nullptr;
        HRESULT hr = swap_chain_->ResizeBuffers(
            0, static_cast<UINT>(display_width_), static_cast<UINT>(display_height_),
            DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT);
        if (FAILED(hr)) {
            LOGF(ERR, "SwapChain resize buffers failed %#x", hr);
            return false;
        }
        if (waitable_obj_) {
            CloseHandle(waitable_obj_);
        }
        swap_chain_->SetMaximumFrameLatency(1);
        waitable_obj_ = swap_chain_->GetFrameLatencyWaitableObject();
        if (!waitable_obj_) {
            LOG(ERR) << "SwapChain GetFrameLatencyWaitableObject failed";
            return false;
        }
        ComPtr<ID3D11Resource> back_buffer;
        hr =
            swap_chain_->GetBuffer(0, __uuidof(ID3D11Resource), (void**)back_buffer.GetAddressOf());
        if (FAILED(hr)) {
            LOGF(ERR, "IDXGISwapChain::GetBuffer failed: %#x", hr);
            return false;
        }
        hr = d3d11_dev_->CreateRenderTargetView(back_buffer.Get(), nullptr, &render_view_);
        if (FAILED(hr)) {
            LOGF(ERR, "ID3D11Device::CreateRenderTargetView failed: %#x", hr);
            return false;
        }
        setupRSStage();
    }
    return true;
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

    if (!setupIAAndVSStage() || !setupRSStage() || !setupPSStage() || !setupOMStage()) {
        return false;
    }
    LOGF(INFO, "d3d11 %u:%u, %u:%u", display_width_, display_height_, video_width_, video_height_);

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
    ComPtr<ID3D11VertexShader> vertex_shader;
    hr = d3d11_dev_->CreateVertexShader(blob->GetBufferPointer(), blob->GetBufferSize(), NULL,
                                        vertex_shader.GetAddressOf());
    if (FAILED(hr)) {
        LOGF(WARNING, "Failed to create vertex shader, hr:0x%08x", hr);
        return false;
    }
    d3d11_ctx_->VSSetShader(vertex_shader.Get(), nullptr, 0);

    ComPtr<ID3D11InputLayout> layout;
    hr = d3d11_dev_->CreateInputLayout(Vertex::input_desc, ARRAYSIZE(Vertex::input_desc),
                                       blob->GetBufferPointer(), blob->GetBufferSize(),
                                       layout.GetAddressOf());
    if (FAILED(hr)) {
        LOGF(WARNING, "Failed to create input layout: %#x", hr);
        return false;
    }
    d3d11_ctx_->IASetInputLayout(layout.Get());
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

    ComPtr<ID3D11Buffer> vertex_buf;
    hr = d3d11_dev_->CreateBuffer(&vb_desc, &vb_data, vertex_buf.GetAddressOf());
    if (FAILED(hr)) {
        LOGF(WARNING, "Failed to create vertext buffer, hr:0x%08x", hr);
        return false;
    }
    UINT stride = sizeof(Vertex);
    UINT offset = 0;
    ID3D11Buffer* vbarray[] = {vertex_buf.Get()};
    // 踩了一个ComPtr陷阱，CPU侧编译运行没有输出任何错误，查了两天。。。
    // IASetVertexBuffers()第三个参数的类型是 ID3D11Buffer *const *ppVertexBuffers
    // 之前的传值写成 &vertex_buf，buffer本身是一个指针，传成它的地址了
    // 而这里其实想要的是一个由buffer指针组成的数组
    d3d11_ctx_->IASetVertexBuffers(0, 1, vbarray, &stride, &offset);

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

    ComPtr<ID3D11Buffer> index_buf;
    hr = d3d11_dev_->CreateBuffer(&index_buffer_desc, &index_buffer_data, index_buf.GetAddressOf());
    if (FAILED(hr)) {
        LOGF(WARNING, "Failed to create index buffer, 0x%08x", hr);
        return false;
    }
    d3d11_ctx_->IASetIndexBuffer(index_buf.Get(), DXGI_FORMAT_R32_UINT, 0);
    d3d11_ctx_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
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
    auto hr = D3DCompile(kPixelShader.c_str(), kPixelShader.size(), NULL, NULL, NULL, "main_PS",
                         "ps_5_0", flags1, 0, blob.GetAddressOf(), NULL);
    if (FAILED(hr)) {
        LOGF(WARNING, "Failed to compile pixel shader, hr:0x%08x", hr);
        return false;
    }
    ComPtr<ID3D11PixelShader> pixel_shader = nullptr;
    hr = d3d11_dev_->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), NULL,
                                       pixel_shader.GetAddressOf());
    if (FAILED(hr)) {
        LOGF(WARNING, "Failed to create pixel shader, hr:0x%08x", hr);
        return false;
    }
    d3d11_ctx_->PSSetShader(pixel_shader.Get(), nullptr, 0);

    D3D11_BUFFER_DESC const_desc = {};
    const_desc.ByteWidth = sizeof(ColorMatrix);
    const_desc.Usage = D3D11_USAGE_IMMUTABLE;
    const_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    const_desc.CPUAccessFlags = 0;
    const_desc.MiscFlags = 0;

    ColorMatrix color_matrix = getColorMatrix();
    D3D11_SUBRESOURCE_DATA const_data = {};
    const_data.pSysMem = &color_matrix;

    ComPtr<ID3D11Buffer> buffer;
    hr = d3d11_dev_->CreateBuffer(&const_desc, &const_data, buffer.GetAddressOf());
    if (FAILED(hr)) {
        LOGF(WARNING, "Failed to create const buffer, hr:0x%08x", hr);
        return false;
    }
    ID3D11Buffer* cbarray[] = {buffer.Get()};
    d3d11_ctx_->PSSetConstantBuffers(0, 1, cbarray);

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

    ComPtr<ID3D11SamplerState> sampler = nullptr;
    hr = d3d11_dev_->CreateSamplerState(&sample_desc, sampler.GetAddressOf());
    if (FAILED(hr)) {
        LOGF(WARNING, "Failed to create sample state, hr:0x%08x", hr);
        return false;
    }
    d3d11_ctx_->PSSetSamplers(0, 1, sampler.GetAddressOf());
    return true;
}

bool D3D11Pipeline::setupOMStage() {
    // 暂时不需要
    return true;
}

bool D3D11Pipeline::initShaderResources(const std::vector<ID3D11Texture2D*>& textures) {
    shader_views_.resize(textures.size());
    CD3D11_SHADER_RESOURCE_VIEW_DESC();
    D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
    srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
    srv_desc.Texture2DArray.MostDetailedMip = 0;
    srv_desc.Texture2DArray.MipLevels = 1;
    srv_desc.Texture2DArray.ArraySize = 1;
    for (size_t i = 0; i < shader_views_.size(); i++) {
        shader_views_[i].texture = textures[i];
        srv_desc.Format = DXGI_FORMAT_R8_UNORM;
        srv_desc.Texture2DArray.FirstArraySlice = static_cast<UINT>(i);
        auto hr = d3d11_dev_->CreateShaderResourceView(textures[i], &srv_desc,
                                                       shader_views_[i].y.GetAddressOf());
        if (FAILED(hr)) {
            LOGF(WARNING, "ID3D11Device::CreateShaderResourceView() failed: 0x%08x", hr);
            return false;
        }
        srv_desc.Format = DXGI_FORMAT_R8G8_UNORM;
        hr = d3d11_dev_->CreateShaderResourceView(textures[i], &srv_desc,
                                                  shader_views_[i].uv.GetAddressOf());
        if (FAILED(hr)) {
            LOGF(WARNING, "ID3D11Device::CreateShaderResourceView() failed: 0x%08x", hr);
            return false;
        }
    }
    return true;
}

const D3D11Pipeline::ColorMatrix& D3D11Pipeline::getColorMatrix() const {
    // TODO: 颜色空间转换系数的选择，应该从编码的时候就确定，下策是从解码器中探知，下下策是乱选一个
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
    for (auto& view : shader_views_) {
        if (view.texture == texture) {
            return view;
        }
    }
    return std::nullopt;
}

} // namespace lt