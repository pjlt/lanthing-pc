// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved

#include <optional>

#include <wrl/client.h>

#include <ltlib/system.h>

#include <video/capturer/dxgi/common_types.h>

#include <dxgi1_6.h>

//
// Handles the task of duplicating an output.
//
class DUPLICATIONMANAGER {
public:
    DUPLICATIONMANAGER();
    ~DUPLICATIONMANAGER();
    _Success_(*Timeout == false && return == DUPL_RETURN_SUCCESS) DUPL_RETURN
        GetFrame(_Out_ FRAME_DATA* Data, _Out_ bool* Timeout);
    DUPL_RETURN DoneWithFrame();
    bool InitDupl(_In_ ID3D11Device* Device, ltlib::Monitor monitor);
    DUPL_RETURN GetMouse(_Inout_ PTR_INFO* PtrInfo, _In_ DXGI_OUTDUPL_FRAME_INFO* FrameInfo,
                         INT OffsetX, INT OffsetY);
    DXGI_OUTPUT_DESC1 GetOutputDesc1();
    void WaitForVBlank();
    bool DefaultOutput();
    bool GetPointerShape(DXGI_OUTDUPL_POINTER_SHAPE_INFO& info, std::vector<uint8_t>& data);

private:
    DUPL_RETURN ResetDulp();
    bool InitDupl2(Microsoft::WRL::ComPtr<IDXGIAdapter> DxgiAdapter, ltlib::Monitor monitor);

private:
    // vars
    Microsoft::WRL::ComPtr<IDXGIOutputDuplication> m_DeskDupl;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> m_AcquiredDesktopImage;
    _Field_size_bytes_(m_MetaDataSize) BYTE* m_MetaDataBuffer;
    UINT m_MetaDataSize;
    UINT m_OutputNumber;
    DXGI_OUTPUT_DESC1 m_OutputDesc;

    Microsoft::WRL::ComPtr<ID3D11Device> m_Device;
    Microsoft::WRL::ComPtr<IDXGIOutput6> m_DxgiOutput;
    bool default_output_ = false;
};