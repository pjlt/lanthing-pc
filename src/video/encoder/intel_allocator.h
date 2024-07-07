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
#include <d3d11.h>
#include <wrl/client.h>

#include <map>
#include <vector>

#include <vpl/mfx.h>

namespace lt {

namespace video {

// TODO: auto manage mids
struct FrameBuffer {
    mfxMemId* mids = nullptr;
    std::vector<Microsoft::WRL::ComPtr<ID3D11Texture2D>> frames;
    ~FrameBuffer() {
        if (mids)
            delete mids;
    }
};

class MfxFrameAllocator : public mfxFrameAllocator {
public:
    MfxFrameAllocator();
    virtual ~MfxFrameAllocator() = default;
    virtual mfxStatus alloc(mfxFrameAllocRequest* request, mfxFrameAllocResponse* response) = 0;
    virtual mfxStatus lock(mfxMemId mid, mfxFrameData* ptr) = 0;
    virtual mfxStatus unlock(mfxMemId mid, mfxFrameData* ptr) = 0;
    virtual mfxStatus get_hdl(mfxMemId mid, mfxHDL* handle) = 0;
    virtual mfxStatus free(mfxFrameAllocResponse* response) = 0;

private:
    static mfxStatus MFX_CDECL _alloc(mfxHDL pthis, mfxFrameAllocRequest* request,
                                      mfxFrameAllocResponse* response);
    static mfxStatus MFX_CDECL _lock(mfxHDL pthis, mfxMemId mid, mfxFrameData* ptr);
    static mfxStatus MFX_CDECL _unlock(mfxHDL pthis, mfxMemId mid, mfxFrameData* ptr);
    static mfxStatus MFX_CDECL _getHDL(mfxHDL pthis, mfxMemId mid, mfxHDL* handle);
    static mfxStatus MFX_CDECL _free(mfxHDL pthis, mfxFrameAllocResponse* response);
};

class MfxEncoderFrameAllocator : public MfxFrameAllocator {
public:
    MfxEncoderFrameAllocator(Microsoft::WRL::ComPtr<ID3D11Device> device,
                             Microsoft::WRL::ComPtr<ID3D11DeviceContext> device_context);
    ~MfxEncoderFrameAllocator() override = default;

    mfxStatus alloc(mfxFrameAllocRequest* request, mfxFrameAllocResponse* response) override;
    mfxStatus lock(mfxMemId mid, mfxFrameData* ptr) override;
    mfxStatus unlock(mfxMemId mid, mfxFrameData* ptr) override;
    mfxStatus get_hdl(mfxMemId mid, mfxHDL* handle) override;
    mfxStatus free(mfxFrameAllocResponse* response) override;

private:
    Microsoft::WRL::ComPtr<ID3D11Device> device_;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> device_context_;
    std::map<mfxMemId*, FrameBuffer> frame_buffers_;
};

// MfxDecoderFrameAllocator没测过，别用
class MfxDecoderFrameAllocator : public MfxFrameAllocator {
public:
    MfxDecoderFrameAllocator(Microsoft::WRL::ComPtr<ID3D11Device> device);
    ~MfxDecoderFrameAllocator() override = default;

    mfxStatus alloc(mfxFrameAllocRequest* request, mfxFrameAllocResponse* response) override;
    mfxStatus lock(mfxMemId mid, mfxFrameData* ptr) override;
    mfxStatus unlock(mfxMemId mid, mfxFrameData* ptr) override;
    mfxStatus get_hdl(mfxMemId mid, mfxHDL* handle) override;
    mfxStatus free(mfxFrameAllocResponse* response) override;

    mfxStatus release_frame(Microsoft::WRL::ComPtr<ID3D11Texture2D> frame);

private:
    mfxStatus alloc_external_frame(mfxFrameAllocRequest* request, mfxFrameAllocResponse* response);
    mfxStatus alloc_internal_frame(mfxFrameAllocRequest* request, mfxFrameAllocResponse* response);

private:
    Microsoft::WRL::ComPtr<ID3D11Device> device_;
    FrameBuffer external_frames_;
    std::map<mfxMemId*, FrameBuffer> internal_frames_;
};

#if 0
class MfxD3D11Allocator : public MfxFrameAllocator {
public:
    MfxD3D11Allocator(Microsoft::WRL::ComPtr<ID3D11Device> device);
    mfxStatus alloc(mfxFrameAllocRequest* request, mfxFrameAllocResponse* response) override;
    mfxStatus lock(mfxMemId mid, mfxFrameData* ptr) override;
    mfxStatus unlock(mfxMemId mid, mfxFrameData* ptr) override;
    mfxStatus get_hdl(mfxMemId mid, mfxHDL* handle) override;
    mfxStatus free(mfxFrameAllocResponse* response) override;

private:
    mfxStatus alloc_external_frame(mfxFrameAllocRequest* request, mfxFrameAllocResponse* response);
    mfxStatus alloc_internal_frame(mfxFrameAllocRequest* request, mfxFrameAllocResponse* response);

private:
    FrameBuffer external_frames_;
    std::map<mfxMemId*, FrameBuffer> internal_frames_;
    Microsoft::WRL::ComPtr<ID3D11Device> device_;
};
#endif

} // namespace video

} // namespace lt