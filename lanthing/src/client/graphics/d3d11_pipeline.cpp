#include "d3d11_pipeline.h"

#include <DirectXMath.h>
#include <d3dcompiler.h>
#include <dwmapi.h>

#include <g3log/g3log.hpp>

#include "pixel_shader.h"
#include "vertex_shader.h"

#define SAFE_COM_RELEASE(x)                                                                        \
    if (x) {                                                                                       \
        (x)->Release();                                                                            \
    }

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "Dwmapi.lib")

namespace lt {

D3D11Pipeline::D3D11Pipeline() {
    DwmEnableMMCSS(TRUE);
}

D3D11Pipeline ::~D3D11Pipeline() {
    SAFE_COM_RELEASE(swap_chain_);
    SAFE_COM_RELEASE(dxgi_factory_);
    SAFE_COM_RELEASE(d3d11_dev_);
    SAFE_COM_RELEASE(d3d11_ctx_);
    uninitDecoder();
}

void D3D11Pipeline::uninitDecoder() {
    while (!frames_.empty()) {
        auto& top = frames_.front();
        av_frame_free(&top.frame);
        frames_.pop_front();
    }
    if (avcodec_context_) {
        avcodec_free_context(&avcodec_context_);
        avcodec_context_ = nullptr;
    }

    if (hw_frames_context_) {
        av_buffer_unref(&hw_frames_context_);
        hw_frames_context_ = nullptr;
    }
    if (hw_device_context_) {
        av_buffer_unref(&hw_device_context_);
        hw_device_context_ = nullptr;
    }
}

// 初始化确定有几个显卡、支持哪些解码方式.
bool D3D11Pipeline::init(size_t index) {
    HRESULT hr;
    DWM_TIMING_INFO info;
    info.cbSize = sizeof(DWM_TIMING_INFO);
    hr = DwmGetCompositionTimingInfo(nullptr, &info);

    if (FAILED(hr) || info.rateRefresh.uiDenominator == 0) {
        LOGF(WARNING, "fail to get dwm composition timing info, hr:0x%08x", hr);
        return false;
    }
    refresh_rate_ = info.rateRefresh.uiNumerator / info.rateRefresh.uiDenominator;

    bool success = false;

    hr = CreateDXGIFactory(__uuidof(IDXGIFactory5), (void**)&dxgi_factory_);
    if (FAILED(hr)) {
        LOGF(WARNING, "fail to create dxgi factory, er:%08x", hr);
        return false;
    }

    IDXGIAdapter1* adapter = nullptr;
    DXGI_ADAPTER_DESC1 adapter_desc;
    hr = dxgi_factory_->EnumAdapters1(index, &adapter);
    if (hr == DXGI_ERROR_NOT_FOUND) {
        // Expected at the end of enumeration
        return false;
    }

    hr = adapter->GetDesc1(&adapter_desc);
    if (FAILED(hr)) {
        false;
    }
    UINT flag = D3D11_CREATE_DEVICE_VIDEO_SUPPORT;
#ifdef _DEBUG
    flag |= D3D11_CREATE_DEVICE_DEBUG;
#endif
    hr = D3D11CreateDevice(adapter, D3D_DRIVER_TYPE_UNKNOWN, nullptr, flag, nullptr, 0,
                           D3D11_SDK_VERSION, &d3d11_dev_, nullptr, &d3d11_ctx_);
    if (FAILED(hr)) {
        LOGF(WARNING, "fail to create d3d11 device, err:%08lx", hr);
        return false;
    }
    adapter_ = index;
    return true;
}

bool D3D11Pipeline::setupRender(HWND hwnd, uint32_t width, uint32_t height) {
    hwnd_ = hwnd;
    video_width_ = width;
    video_height_ = height;
    RECT rect;
    if (!GetClientRect(hwnd, &rect)) {
        return false;
    }
    display_width_ = rect.right - rect.left;
    display_height_ = rect.bottom - rect.top;
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.Stereo = FALSE;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.SampleDesc.Quality = 0;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
    swapChainDesc.Flags = 0;
    swapChainDesc.Width = display_width_;
    swapChainDesc.Height = display_height_;
    swapChainDesc.BufferCount = 3 + 1 + 1;
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

    IDXGISwapChain1* swapChain;
    auto hr = dxgi_factory_->CreateSwapChainForHwnd(d3d11_dev_, hwnd_, &swapChainDesc, nullptr,
                                                    nullptr, &swapChain);
    if (FAILED(hr)) {
        LOGF(INFO, "fail to create swap chain");
        return false;
    }
    hr = swapChain->QueryInterface(__uuidof(IDXGISwapChain4), (void**)&swap_chain_);
    swapChain->Release();
    if (FAILED(hr)) {
        LOGF(WARNING, "IDXGISwapChain::QueryInterface(IDXGISwapChain4) failed : % x ", hr);
        return false;
    }
    hr = dxgi_factory_->MakeWindowAssociation(hwnd_, DXGI_MWA_NO_WINDOW_CHANGES);
    if (FAILED(hr)) {
        LOGF(WARNING, "IDXGIFactory::MakeWindowAssociation() failed: %x", hr);
        return false;
    }

    // Create our render target view
    ID3D11Resource* backBufferResource;
    hr = swap_chain_->GetBuffer(0, __uuidof(ID3D11Resource), (void**)&backBufferResource);
    if (FAILED(hr)) {
        LOGF(WARNING, "IDXGISwapChain::GetBuffer failed, err: %x", hr);
        return false;
    }
    hr = d3d11_dev_->CreateRenderTargetView(backBufferResource, nullptr, &render_view_);
    backBufferResource->Release();
    if (FAILED(hr)) {
        LOGF(WARNING, "ID3D11Device::CreateRenderTargetView failed: %x", hr);
        return false;
    }

    if (!setupIAAndVSStage() || !setupRSStage() || !setupPSStage() || !setupOMStage()) {
        return false;
    }
    LOGF(INFO, "d3d11 %u:%u, %u:%u", display_width_, display_height_, video_width_, video_height_);
    return true;
}

struct Vertex {
    DirectX::XMFLOAT2 pos;
    DirectX::XMFLOAT2 color;
    static const D3D11_INPUT_ELEMENT_DESC input_desc[2];
};
const D3D11_INPUT_ELEMENT_DESC Vertex::input_desc[2] = {
    {"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
    {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 8, D3D11_INPUT_PER_VERTEX_DATA, 0},
};

bool D3D11Pipeline::setupIAAndVSStage() {
    d3d11_ctx_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ID3DBlob* blob = nullptr;
    auto hr = D3DCompile(d3d11_vertex_shader.c_str(), d3d11_vertex_shader.size(), NULL, NULL, NULL,
                         "main", "vs_4_0", 0, 0, &blob, NULL);
    if (FAILED(hr)) {
        LOGF(WARNING, "fail to compile vertex shader, hr:0x%08x", hr);
        return false;
    }
    ID3D11VertexShader* vertex_shader = nullptr;
    hr = d3d11_dev_->CreateVertexShader(blob->GetBufferPointer(), blob->GetBufferSize(), NULL,
                                        &vertex_shader);
    if (FAILED(hr)) {
        LOGF(WARNING, "fail to create vertex shader, hr:0x%08x", hr);
        return false;
    }
    d3d11_ctx_->VSSetShader(vertex_shader, nullptr, 0);
    vertex_shader->Release();

    ID3D11InputLayout* layout = nullptr;
    hr = d3d11_dev_->CreateInputLayout(Vertex::input_desc, ARRAYSIZE(Vertex::input_desc),
                                       blob->GetBufferPointer(), blob->GetBufferSize(), &layout);
    blob->Release();
    if (FAILED(hr)) {
        LOGF(WARNING, "fail to create input layout");
        return false;
    }
    d3d11_ctx_->IASetInputLayout(layout);
    layout->Release();

    // padding区域
    int align = codec_ == Codec::VIDEO_H264 ? 16 : 128;
    float u = (float)video_width_ / FFALIGN(video_width_, align);
    float v = (float)video_height_ / FFALIGN(video_height_, align);

    Vertex verts[] = {
        {DirectX::XMFLOAT2{-1.0f, -1.0f}, DirectX::XMFLOAT2{0, v}},
        {DirectX::XMFLOAT2{-1.0f, 1.0f}, DirectX::XMFLOAT2{0, 0}},
        {DirectX::XMFLOAT2{1.0f, -1.0f}, DirectX::XMFLOAT2{u, v}},
        {DirectX::XMFLOAT2{1.0f, 1.0f}, DirectX::XMFLOAT2{u, 0}},
    };

    D3D11_BUFFER_DESC vbDesc = {};
    vbDesc.ByteWidth = sizeof(verts);
    vbDesc.Usage = D3D11_USAGE_IMMUTABLE;
    vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vbDesc.CPUAccessFlags = 0;
    vbDesc.MiscFlags = 0;
    vbDesc.StructureByteStride = sizeof(Vertex);

    D3D11_SUBRESOURCE_DATA vbData = {};
    vbData.pSysMem = verts;

    ID3D11Buffer* vertex_buf = nullptr;
    hr = d3d11_dev_->CreateBuffer(&vbDesc, &vbData, &vertex_buf);
    if (FAILED(hr)) {
        LOGF(WARNING, "fail to create vertext buffer, hr:0x%08x", hr);
        return false;
    }
    // Bind video rendering vertex buffer
    UINT stride = sizeof(Vertex);
    UINT offset = 0;
    d3d11_ctx_->IASetVertexBuffers(0, 1, &vertex_buf, &stride, &offset);
    vertex_buf->Release();

    // 索引
    const int indexes[] = {0, 1, 2, 3, 2, 1};
    D3D11_BUFFER_DESC indexBufferDesc = {};
    indexBufferDesc.ByteWidth = sizeof(indexes);
    indexBufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
    indexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    indexBufferDesc.CPUAccessFlags = 0;
    indexBufferDesc.MiscFlags = 0;
    indexBufferDesc.StructureByteStride = sizeof(int);

    D3D11_SUBRESOURCE_DATA indexBufferData = {};
    indexBufferData.pSysMem = indexes;
    indexBufferData.SysMemPitch = sizeof(int);

    ID3D11Buffer* index_buf;
    hr = d3d11_dev_->CreateBuffer(&indexBufferDesc, &indexBufferData, &index_buf);
    if (FAILED(hr)) {
        LOGF(WARNING, "fail to create index buffer, 0x%08x", hr);
        return false;
    }
    d3d11_ctx_->IASetIndexBuffer(index_buf, DXGI_FORMAT_R32_UINT, 0);
    index_buf->Release();

    return true;
}

bool D3D11Pipeline::setupRSStage() {
    D3D11_VIEWPORT viewport;
    viewport.TopLeftX = 0;
    viewport.TopLeftY = 0;
    viewport.Width = (float)display_width_;
    viewport.Height = (float)display_height_;
    viewport.MinDepth = 0;
    viewport.MaxDepth = 1;
    d3d11_ctx_->RSSetViewports(1, &viewport);

    return true;
}

typedef struct _CSC_CONST_BUF {
    // CscMatrix value from above but packed appropriately
    float cscMatrix[12];

    // YUV offset values from above
    float offsets[3];

    // Padding float to be a multiple of 16 bytes
    float padding;
} CSC_CONST_BUF;
static_assert(sizeof(CSC_CONST_BUF) % 16 == 0, "Constant buffer sizes must be a multiple of 16");

bool D3D11Pipeline::setupPSStage() {
    ID3DBlob* blob = nullptr;
    auto hr = D3DCompile(d3d11_pixel_shader.c_str(), d3d11_pixel_shader.size(), NULL, NULL, NULL,
                         "main", "ps_4_0", 0, 0, &blob, NULL);
    if (FAILED(hr)) {
        LOGF(WARNING, "fail to compile pixel shader, hr:0x%08x", hr);
        return false;
    }
    ID3D11PixelShader* pixel_shader = nullptr;
    hr = d3d11_dev_->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), NULL,
                                       &pixel_shader);
    if (FAILED(hr)) {
        LOGF(WARNING, "fail to create vertex shader, hr:0x%08x", hr);
        return false;
    }
    d3d11_ctx_->PSSetShader(pixel_shader, nullptr, 0);
    pixel_shader->Release();

    D3D11_BUFFER_DESC constDesc = {};
    constDesc.ByteWidth = sizeof(CSC_CONST_BUF);
    constDesc.Usage = D3D11_USAGE_IMMUTABLE;
    constDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    constDesc.CPUAccessFlags = 0;
    constDesc.MiscFlags = 0;

    CSC_CONST_BUF const_fuf = {};
    const float bt709[9] = {
        1.1644f, 1.1644f, 1.1644f, 0.0f, -0.3917f, 2.0172f, 1.5960f, -0.8129f, 0.0f,
    };
    // We need to adjust our raw CSC matrix to be column-major and with float3 vectors
    // padded with a float in between each of them to adhere to HLSL requirements.
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            const_fuf.cscMatrix[i * 4 + j] = bt709[j * 3 + i];
        }
    }
    const float offsets[3] = {16.0f / 255.0f, 128.0f / 255.0f, 128.0f / 255.0f};
    memcpy(const_fuf.offsets, offsets, sizeof(const_fuf.offsets));
    D3D11_SUBRESOURCE_DATA constData = {};
    constData.pSysMem = &const_fuf;

    ID3D11Buffer* buffer = nullptr;
    hr = d3d11_dev_->CreateBuffer(&constDesc, &constData, &buffer);
    if (FAILED(hr)) {
        LOGF(WARNING, "fail to create const buffer, hr:0x%08x", hr);
        return false;
    }
    d3d11_ctx_->PSSetConstantBuffers(0, 1, &buffer);
    buffer->Release();

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

    ID3D11SamplerState* sampler = nullptr;
    hr = d3d11_dev_->CreateSamplerState(&sample_desc, &sampler);
    if (FAILED(hr)) {
        LOGF(WARNING, "fail to create sample state, hr:0x%08x", hr);
        return false;
    }
    d3d11_ctx_->PSSetSamplers(0, 1, &sampler);
    sampler->Release();
    return true;
}

bool D3D11Pipeline::setupOMStage() {
    D3D11_BLEND_DESC blendDesc = {};
    blendDesc.AlphaToCoverageEnable = FALSE;
    blendDesc.IndependentBlendEnable = FALSE;
    blendDesc.RenderTarget[0].BlendEnable = TRUE;
    blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
    blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

    ID3D11BlendState* blendState = nullptr;
    auto hr = d3d11_dev_->CreateBlendState(&blendDesc, &blendState);
    if (FAILED(hr)) {
        LOGF(WARNING, "fail to create blend state, hr:0x%08x", hr);
        return false;
    }

    d3d11_ctx_->OMSetBlendState(blendState, nullptr, 0xffffffff);

    blendState->Release();
    return true;
}

void mapTextureToFile(ID3D11Device* d3d11_dev, ID3D11DeviceContext* d3d11_context,
                      ID3D11Texture2D* texture, size_t index) {
    static ID3D11Texture2D* cpu_texture = NULL;
    if (!cpu_texture) {
        D3D11_TEXTURE2D_DESC desc;
        ZeroMemory(&desc, sizeof(D3D11_TEXTURE2D_DESC));
        desc.Width = 1920;
        desc.Height = 1080;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_NV12;
        desc.SampleDesc.Count = 1;
        desc.BindFlags = 0;
        desc.MiscFlags = 0;
        // Create CPU access texture
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
        desc.Usage = D3D11_USAGE_STAGING;
        if (d3d11_dev->CreateTexture2D(&desc, NULL, &cpu_texture) != S_OK) {
            return;
        }
    }
    d3d11_context->CopySubresourceRegion(cpu_texture, 0, 0, 0, 0, texture, index, nullptr);
    D3D11_MAPPED_SUBRESOURCE resource;
    UINT subresource = D3D11CalcSubresource(0, 0, 0);
    auto ret = d3d11_context->Map(cpu_texture, subresource, D3D11_MAP_READ_WRITE, 0, &resource);
    if (!SUCCEEDED(ret)) {
        LOGF(WARNING, "fail to map texture");
        return;
    }
    uint8_t* dptr = reinterpret_cast<uint8_t*>(resource.pData);
    // 保存argb数据
    static std::FILE* file = std::fopen("decoded.nv12", "wb");
    for (size_t i = 0; i < 1080 * 3 / 2; i++) {
        std::fwrite(dptr + i * resource.RowPitch, 1920, 1, file);
    }
    d3d11_context->Unmap(cpu_texture, subresource);
}

bool D3D11Pipeline::render(int64_t resouce) {
    // LOGF(INFO, "render resouce %lld", resouce);
    auto frame = get(resouce);
    if (!frame) {
        LOGF(WARNING, "can not find resouce[%lld]", resouce);
        return false;
    }
    size_t index = frame->index;
    erase(resouce);
    // mapTextureToFile(d3d11_dev_, d3d11_ctx_, frame->texture, frame->index);
    std::lock_guard<std::mutex> lock(pipeline_mtx_);
    const float clear[4] = {0.f, 0.f, 0.f, 0.f};
    d3d11_ctx_->ClearRenderTargetView(render_view_, clear);
    d3d11_ctx_->OMSetRenderTargets(1, &render_view_, nullptr);
    d3d11_ctx_->PSSetShaderResources(0, 2, shader_views_[index].array.data());
    d3d11_ctx_->DrawIndexed(6, 0, 0);

    auto hr = swap_chain_->Present(0, 0);

    if (FAILED(hr)) {
        LOGF(WARNING, "failed to call presenter, hr:0x%08x", hr);
        return false;
    }
    return true;
}

bool D3D11Pipeline::setupDecoder(Format format) {
    codec_ = Codec::VIDEO_H265;
    format_ = format;
    assert(codec_ == Codec::VIDEO_H264 || codec_ == Codec::VIDEO_H265);
    auto codec_id = codec_ == Codec::VIDEO_H264 ? AV_CODEC_ID_H264 : AV_CODEC_ID_H265;

    if (!checkDecoder()) {
        return false;
    }
    LOGF(INFO, "adapter %zu support %s", adapter_, videoFormatToString(format_).c_str());

    const AVCodec* decoder = avcodec_find_decoder(codec_id);
    if (!decoder) {
        LOGF(WARNING, "can't bind av decoder");
        return false;
    }

    while (frames_.size() < av_pool_size_) {
        auto av_frame = av_frame_alloc();
        if (!av_frame) {
            break;
        }
        auto pkt = av_packet_alloc();
        if (!pkt) {
            av_frame_free(&av_frame);
            break;
        }
        Frame frame;
        frame.pkt = pkt;
        frame.frame = av_frame;
        frames_.push_back(frame);
    }
    av_pool_size_ = frames_.size();

    for (int i = 0;; i++) {
        const AVCodecHWConfig* config = avcodec_get_hw_config(decoder, i);
        if (!config) {
            // No remaing hwaccel options
            avcodec_hwconfig_ = nullptr;
            return false;
        }

        if (!(config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX) ||
            config->device_type != AV_HWDEVICE_TYPE_D3D11VA) {
            continue;
        }
        avcodec_hwconfig_ = config;
        if (!initDecoderContext()) {
            if (hw_frames_context_) {
                av_buffer_unref(&hw_frames_context_);
                hw_frames_context_ = nullptr;
            }
            if (hw_device_context_) {
                av_buffer_unref(&hw_device_context_);
                hw_device_context_ = nullptr;
            }
            continue;
        }
        if (!initAVCodec(decoder)) {
            continue;
        }
        break;
    }

    return true;
}

bool D3D11Pipeline::checkDecoder() {
    assert(format_ != Format::UNSUPPORT);
    ID3D11VideoDevice* video_device = nullptr;
    auto hr = d3d11_dev_->QueryInterface(__uuidof(ID3D11VideoDevice), (void**)&video_device);
    if (FAILED(hr)) {
        return false;
    }
    GUID guid;
    DXGI_FORMAT format;
    if (format_ == Format::H264_NV12 || format_ == Format::H264_YUV444) {
        guid = D3D11_DECODER_PROFILE_H264_VLD_NOFGT;
        format = format_ == Format::H264_NV12 ? DXGI_FORMAT_NV12 : DXGI_FORMAT_AYUV;
    }
    else if (format_ == Format::H265_NV12 || format_ == Format::H265_YUV444) {
        guid = D3D11_DECODER_PROFILE_HEVC_VLD_MAIN;
        format = format_ == Format::H265_NV12 ? DXGI_FORMAT_NV12 : DXGI_FORMAT_AYUV;
    }
    else {
        assert(false);
    }
    BOOL supported = false;
    hr = video_device->CheckVideoDecoderFormat(&guid, format, &supported);
    video_device->Release();
    if (FAILED(hr) || !supported) {
        LOGF(INFO, "%s is not supported", videoFormatToString(format_).c_str());
        return false;
    }

    return true;
}

bool D3D11Pipeline::initAVCodec(const AVCodec* decoder) {
    avcodec_context_ = avcodec_alloc_context3(decoder);
    if (!avcodec_context_) {
        return false;
    }
    avcodec_context_->flags |= AV_CODEC_FLAG_LOW_DELAY;
    avcodec_context_->flags |= AV_CODEC_FLAG_OUTPUT_CORRUPT;
    avcodec_context_->flags2 |= AV_CODEC_FLAG2_SHOW_ALL;
    // Report decoding errors to allow us to request a key frame
    avcodec_context_->err_recognition = AV_EF_EXPLODE;
    avcodec_context_->thread_count = 1;
    // Setup decoding parameters
    avcodec_context_->width = video_width_;
    avcodec_context_->height = video_height_;
    avcodec_context_->pix_fmt = AVPixelFormat::AV_PIX_FMT_YUV420P;
    avcodec_context_->get_format = getFormat;
    avcodec_context_->width = video_width_;
    avcodec_context_->opaque = this;

    int err = avcodec_open2(avcodec_context_, decoder, nullptr);
    if (err < 0) {
        LOGF(WARNING, "fail to open avcodec, err:%d", err);
        avcodec_free_context(&avcodec_context_);
        avcodec_context_ = nullptr;
        return false;
    }
    return true;
}

void D3D11Pipeline::d3d11LockContext(void* ctx) {
    //((D3D11Pipeline*)ctx)->pipeline_mtx_.lock();
}

void D3D11Pipeline::d3d11UnlockContext(void* ctx) {
    //((D3D11Pipeline*)ctx)->pipeline_mtx_.unlock();
}

bool D3D11Pipeline::initDecoderContext() {
    hw_device_context_ = av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_D3D11VA);
    if (!hw_device_context_) {
        LOGF(WARNING, "fail to alloc hw device context");
        return false;
    }

    AVHWDeviceContext* deviceContext = (AVHWDeviceContext*)hw_device_context_->data;
    AVD3D11VADeviceContext* d3d11vaDeviceContext = (AVD3D11VADeviceContext*)deviceContext->hwctx;
    // AVHWDeviceContext takes ownership of these objects
    d3d11vaDeviceContext->device = d3d11_dev_;
    d3d11vaDeviceContext->device_context = d3d11_ctx_;
    d3d11_dev_->AddRef();
    d3d11_ctx_->AddRef();

    // Set lock functions that we will use to synchronize with FFmpeg's usage of our device context
    d3d11vaDeviceContext->lock = d3d11LockContext;
    d3d11vaDeviceContext->unlock = d3d11UnlockContext;
    d3d11vaDeviceContext->lock_ctx = this;

    int err = av_hwdevice_ctx_init(hw_device_context_);
    if (err < 0) {
        LOGF(WARNING, "fail to init hw device context, err:%d", err);
        return false;
    }

    hw_frames_context_ = av_hwframe_ctx_alloc(hw_device_context_);
    if (!hw_frames_context_) {
        LOGF(WARNING, "fail to allocate D3D11VA frame context");
        return false;
    }

    AVHWFramesContext* framesContext = (AVHWFramesContext*)hw_frames_context_->data;
    framesContext->format = AV_PIX_FMT_D3D11;
    framesContext->sw_format = AV_PIX_FMT_NV12;
    int align = codec_ == Codec::VIDEO_H264 ? 16 : 128;
    framesContext->width = FFALIGN(video_width_, align);
    framesContext->height = FFALIGN(video_height_, align);
    framesContext->initial_pool_size = av_pool_size_;

    AVD3D11VAFramesContext* d3d11vaFramesContext = (AVD3D11VAFramesContext*)framesContext->hwctx;
    d3d11vaFramesContext->BindFlags = D3D11_BIND_DECODER | D3D11_BIND_SHADER_RESOURCE;
    err = av_hwframe_ctx_init(hw_frames_context_);
    if (err < 0) {
        LOGF(WARNING, "fail to initialize D3D11VA frame context,err:%d", err);
        return false;
    }
    std::vector<ID3D11Texture2D*> textures(av_pool_size_);
    for (size_t i = 0; i < av_pool_size_; i++) {
        auto txt = d3d11vaFramesContext->texture_infos[i];
        textures[txt.index] = txt.texture;
    }
    if (!initShaderResources(textures)) {
        LOGF(WARNING, "fail to initialize shader resources");
        return false;
    }

    return true;
}

bool D3D11Pipeline::initShaderResources(std::vector<ID3D11Texture2D*> textures) {
    assert(av_pool_size_ == textures.size());
    shader_views_.resize(av_pool_size_);
    for (auto& view : shader_views_) {
        view.array.resize(2);
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
    srvDesc.Texture2DArray.MostDetailedMip = 0;
    srvDesc.Texture2DArray.MipLevels = 1;
    srvDesc.Texture2DArray.ArraySize = 1;
    // Create luminance and chrominance SRVs for each texture in the pool
    for (int i = 0; i < shader_views_.size(); i++) {
        srvDesc.Format = DXGI_FORMAT_R8_UNORM;
        srvDesc.Texture2DArray.FirstArraySlice = i;
        auto hr =
            d3d11_dev_->CreateShaderResourceView(textures[i], &srvDesc, &shader_views_[i].array[0]);
        if (FAILED(hr)) {
            LOGF(WARNING, "ID3D11Device::CreateShaderResourceView() failed: 0x%08x", hr);
            shader_views_[i].array[0] = nullptr;
            return false;
        }
        srvDesc.Format = DXGI_FORMAT_R8G8_UNORM;
        hr =
            d3d11_dev_->CreateShaderResourceView(textures[i], &srvDesc, &shader_views_[i].array[1]);
        if (FAILED(hr)) {
            LOGF(WARNING, "ID3D11Device::CreateShaderResourceView() failed: 0x%08x", hr);
            shader_views_[i].array[1] = nullptr;
            return false;
        }
    }
    return true;
}

enum AVPixelFormat D3D11Pipeline::getFormat(AVCodecContext* context,
                                            const enum AVPixelFormat* pixFmts) {
    D3D11Pipeline* decoder = (D3D11Pipeline*)context->opaque;
    const enum AVPixelFormat* p;
    for (p = pixFmts; *p != -1; p++) {
        // Only match our hardware decoding codec or preferred SW pixel
        // format (if not using hardware decoding). It's crucial
        // to override the default get_format() which will try
        // to gracefully fall back to software decode and break us.
        if (*p ==
            (decoder->avcodec_hwconfig_ ? decoder->avcodec_hwconfig_->pix_fmt : context->pix_fmt)) {
            context->hw_frames_ctx = av_buffer_ref(decoder->hw_frames_context_);
            return *p;
        }
    }

    return AV_PIX_FMT_NONE;
}

int64_t D3D11Pipeline::decode(const uint8_t* data, uint32_t size) {
    std::lock_guard<std::mutex> lock1(pipeline_mtx_);
    std::lock_guard<std::mutex> lock(frames_mtx_);
    auto& front = frames_.front();
    av_packet_unref(front.pkt);
    av_frame_unref(front.frame);

    front.pkt->data = const_cast<uint8_t*>(data);
    front.pkt->size = size;

    int err = avcodec_send_packet(avcodec_context_, front.pkt);
    if (FAILED(err)) {
        LOGF(WARNING, "fail to call avcodec_send_packet, err:%d", err);
        av_packet_unref(front.pkt);
        return -1;
    }

    err = avcodec_receive_frame(avcodec_context_, front.frame);
    if (FAILED(err)) {
        LOGF(WARNING, "fail to call avcodec_receive_frame, err:%d", err);
        av_packet_unref(front.pkt);
        av_frame_unref(front.frame);
        return -1;
    }
    Frame frame;
    frame.id = id_counter_++;
    frame.index = static_cast<size_t>((uintptr_t)front.frame->data[1]);
    assert(frame.index < frames_.size());
    frame.texture = (ID3D11Texture2D*)front.frame->data[0];
    frame.pkt = front.pkt;
    frame.frame = front.frame;
    decoded_frames_.emplace(frame.id, frame);

    frames_.push_back(frame);
    frames_.pop_front();
    frames_.shrink_to_fit();

    return frame.id;
}

void D3D11Pipeline::erase(int64_t frame_id) {
    std::lock_guard<std::mutex> lock(frames_mtx_);
    auto iter = decoded_frames_.lower_bound(frame_id + 1);
    decoded_frames_.erase(decoded_frames_.begin(), iter);
}

const D3D11Pipeline::Frame* D3D11Pipeline::get(int64_t frame_id) {
    std::lock_guard<std::mutex> lock(frames_mtx_);
    bool cached = false;
    for (const auto& iter : frames_) {
        if (iter.id == frame_id) {
            cached = true;
            break;
        }
    }
    if (!cached) {
        return nullptr;
    }
    auto iter = decoded_frames_.find(frame_id);
    if (iter == decoded_frames_.end()) {
        return false;
    }
    return &(iter->second);
}

} // namespace lt