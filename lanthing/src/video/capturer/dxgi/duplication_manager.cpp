// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved

#include "duplication_manager.h"

#include <ltlib/logging.h>
#include <ltlib/pragma_warning.h>
#include <ltlib/system.h>

WARNING_DISABLE(6101)

using namespace Microsoft::WRL;

//
// Constructor sets up references / variables
//
DUPLICATIONMANAGER::DUPLICATIONMANAGER()
    : m_MetaDataBuffer(nullptr)
    , m_MetaDataSize(0)
    , m_OutputNumber(0) {
    RtlZeroMemory(&m_OutputDesc, sizeof(m_OutputDesc));
}

//
// Destructor simply calls CleanRefs to destroy everything
//
DUPLICATIONMANAGER::~DUPLICATIONMANAGER() {
    if (m_MetaDataBuffer) {
        delete[] m_MetaDataBuffer;
        m_MetaDataBuffer = nullptr;
    }
}

//
// Initialize duplication interfaces
//
bool DUPLICATIONMANAGER::InitDupl(_In_ ID3D11Device* Device, ltlib::Monitor monitor) {
    // Take a reference on the device
    m_Device = Device;

    // Get DXGI device
    ComPtr<IDXGIDevice> DxgiDevice = nullptr;
    HRESULT hr = m_Device->QueryInterface(__uuidof(IDXGIDevice),
                                          reinterpret_cast<void**>(DxgiDevice.GetAddressOf()));
    if (FAILED(hr)) {
        LOGF(ERR, "failed to get DXGI Device, hr: 0x%08x", hr);
        return false;
    }

    // Get DXGI adapter
    ComPtr<IDXGIAdapter> DxgiAdapter = nullptr;
    hr = DxgiDevice->GetParent(__uuidof(IDXGIAdapter),
                               reinterpret_cast<void**>(DxgiAdapter.GetAddressOf()));
    if (FAILED(hr)) {
        LOGF(ERR, "failed to get parent DXGI Adapter, hr: 0x%08x", hr);
        return false;
    }
    if (!InitDupl2(DxgiAdapter, monitor)) {
        SetThreadExecutionState(ES_DISPLAY_REQUIRED);
        std::this_thread::sleep_for(std::chrono::milliseconds{500});
        if (!InitDupl2(DxgiAdapter, monitor)) {
            LOG(ERR) << "InitDupl2 failed 2 times";
            return false;
        }
    }
    return true;
}

bool DUPLICATIONMANAGER::InitDupl2(Microsoft::WRL::ComPtr<IDXGIAdapter> DxgiAdapter,
                                   ltlib::Monitor monitor) {
    m_OutputNumber = std::numeric_limits<decltype(m_OutputNumber)>::max();
    ComPtr<IDXGIOutput> DxgiOutput;
    ComPtr<IDXGIOutputDuplication> DeskDupl;
    HRESULT hr;
    // Get output
    for (UINT i = 0;; i++) {
        ComPtr<IDXGIOutput> dxgi_output;
        DXGI_OUTPUT_DESC desc{};
        hr = DxgiAdapter->EnumOutputs(i, dxgi_output.GetAddressOf());
        if (hr == DXGI_ERROR_NOT_FOUND) {
            break;
        }
        if (FAILED(hr)) {
            LOGF(ERR, "IDXGIOutput::EnumOutputs(%u) failed with %#x", i, hr);
            return false;
        }
        dxgi_output->GetDesc(&desc);
        if (desc.DesktopCoordinates.left == monitor.left &&
            desc.DesktopCoordinates.top == monitor.top &&
            desc.DesktopCoordinates.right == monitor.right &&
            desc.DesktopCoordinates.bottom == monitor.bottom) {
            LOG(INFO) << "Found match output " << i;
            m_OutputNumber = i;
            m_OutputDesc = desc;
            DxgiOutput = dxgi_output;
            default_output_ = false;
            break;
        }
    }
    if (m_OutputNumber == std::numeric_limits<decltype(m_OutputNumber)>::max()) {
        LOG(WARNING) << "No match output, use default one";
        hr = DxgiAdapter->EnumOutputs(0, DxgiOutput.GetAddressOf());
        if (FAILED(hr)) {
            LOGF(ERR, "IDXGIOutput::EnumOutputs(%d) failed with %#x", 0, hr);
            return false;
        }
        DxgiOutput->GetDesc(&m_OutputDesc);
        default_output_ = true;
    }
    if (DxgiOutput == nullptr) {
        LOG(INFO) << "DxgiOutput == nullptr";
        return false;
    }
    // QI for Output 1
    ComPtr<IDXGIOutput1> DxgiOutput1;
    hr = DxgiOutput->QueryInterface(__uuidof(IDXGIOutput1),
                                    reinterpret_cast<void**>(DxgiOutput1.GetAddressOf()));
    if (FAILED(hr)) {
        LOGF(ERR, "Failed to QI for DxgiOutput1 in DUPLICATIONMANAGER, hr: 0x%08x", hr);
        return false;
    }

    // Create desktop duplication
    hr = DxgiOutput1->DuplicateOutput(m_Device.Get(), DeskDupl.GetAddressOf());
    if (FAILED(hr)) {
        if (hr == DXGI_ERROR_NOT_CURRENTLY_AVAILABLE) {
            LOGF(ERR, "There is already the maximum number of applications using the Desktop "
                      "Duplication API running, please close one of those applications and then "
                      "try again.");
        }
        LOGF(ERR, "Failed to call DuplicateOutput, hr:0x%08x", hr);
        return false;
    }
    m_DeskDupl = DeskDupl;
    m_DxgiOutput = DxgiOutput;
    return true;
}

//
// Retrieves mouse info and write it into PtrInfo
//
DUPL_RETURN DUPLICATIONMANAGER::GetMouse(_Inout_ PTR_INFO* PtrInfo,
                                         _In_ DXGI_OUTDUPL_FRAME_INFO* FrameInfo, INT OffsetX,
                                         INT OffsetY) {
    // A non-zero mouse update timestamp indicates that there is a mouse position update and
    // optionally a shape change
    if (FrameInfo->LastMouseUpdateTime.QuadPart == 0) {
        return DUPL_RETURN_SUCCESS;
    }

    bool UpdatePosition = true;

    // Make sure we don't update pointer position wrongly
    // If pointer is invisible, make sure we did not get an update from another output that the last
    // time that said pointer was visible, if so, don't set it to invisible or update.
    if (!FrameInfo->PointerPosition.Visible &&
        (PtrInfo->WhoUpdatedPositionLast != m_OutputNumber)) {
        UpdatePosition = false;
    }

    // If two outputs both say they have a visible, only update if new update has newer timestamp
    if (FrameInfo->PointerPosition.Visible && PtrInfo->Visible &&
        (PtrInfo->WhoUpdatedPositionLast != m_OutputNumber) &&
        (PtrInfo->LastTimeStamp.QuadPart > FrameInfo->LastMouseUpdateTime.QuadPart)) {
        UpdatePosition = false;
    }

    // Update position
    if (UpdatePosition) {
        PtrInfo->Position.x =
            FrameInfo->PointerPosition.Position.x + m_OutputDesc.DesktopCoordinates.left - OffsetX;
        PtrInfo->Position.y =
            FrameInfo->PointerPosition.Position.y + m_OutputDesc.DesktopCoordinates.top - OffsetY;
        PtrInfo->WhoUpdatedPositionLast = m_OutputNumber;
        PtrInfo->LastTimeStamp = FrameInfo->LastMouseUpdateTime;
        PtrInfo->Visible = FrameInfo->PointerPosition.Visible != 0;
    }

    // No new shape
    if (FrameInfo->PointerShapeBufferSize == 0) {
        return DUPL_RETURN_SUCCESS;
    }

    // Old buffer too small
    if (FrameInfo->PointerShapeBufferSize > PtrInfo->BufferSize) {
        if (PtrInfo->PtrShapeBuffer) {
            delete[] PtrInfo->PtrShapeBuffer;
            PtrInfo->PtrShapeBuffer = nullptr;
        }
        PtrInfo->PtrShapeBuffer = new (std::nothrow) BYTE[FrameInfo->PointerShapeBufferSize];
        if (!PtrInfo->PtrShapeBuffer) {
            PtrInfo->BufferSize = 0;
            // return ProcessFailure(
            //     nullptr, L"Failed to allocate memory for pointer shape in DUPLICATIONMANAGER",
            //     L"Error", E_OUTOFMEMORY);
            return DUPL_RETURN::DUPL_RETURN_ERROR_UNEXPECTED;
        }

        // Update buffer size
        PtrInfo->BufferSize = FrameInfo->PointerShapeBufferSize;
    }

    // Get shape
    UINT BufferSizeRequired;
    HRESULT hr = m_DeskDupl->GetFramePointerShape(FrameInfo->PointerShapeBufferSize,
                                                  reinterpret_cast<VOID*>(PtrInfo->PtrShapeBuffer),
                                                  &BufferSizeRequired, &(PtrInfo->ShapeInfo));
    if (FAILED(hr)) {
        delete[] PtrInfo->PtrShapeBuffer;
        PtrInfo->PtrShapeBuffer = nullptr;
        PtrInfo->BufferSize = 0;
        // return ProcessFailure(m_Device, L"Failed to get frame pointer shape in
        // DUPLICATIONMANAGER",
        //                       L"Error", hr, FrameInfoExpectedErrors);
        return DUPL_RETURN::DUPL_RETURN_ERROR_UNEXPECTED;
    }

    return DUPL_RETURN_SUCCESS;
}

#pragma warning(disable : 6101)
//
// Get next frame and write it into Data
//
_Success_(*Timeout == false && return == DUPL_RETURN_SUCCESS)
    DUPL_RETURN DUPLICATIONMANAGER::GetFrame(_Out_ FRAME_DATA* Data, _Out_ bool* Timeout) {
    if (m_DeskDupl == nullptr) {
        return DUPL_RETURN::DUPL_RETURN_ERROR_UNEXPECTED;
    }
    ComPtr<IDXGIResource> DesktopResource;
    DXGI_OUTDUPL_FRAME_INFO FrameInfo;
    // Get new frame
    HRESULT hr = m_DeskDupl->AcquireNextFrame(50, &FrameInfo, DesktopResource.GetAddressOf());
    if (hr == DXGI_ERROR_WAIT_TIMEOUT) {
        *Timeout = true;
        LOG(DEBUG) << "Dupl timeout";
        return DUPL_RETURN_SUCCESS;
    }
    *Timeout = false;

    if (hr == DXGI_ERROR_ACCESS_LOST) {
        (void)Data;
        LOG(ERR) << "DXGI_ERROR_ACCESS_LOST";
        return ResetDulp();
    }

    if (FAILED(hr)) {
        // return ProcessFailure(m_Device, L"Failed to acquire next frame in DUPLICATIONMANAGER",
        //                       L"Error", hr, FrameInfoExpectedErrors);
        LOGF(ERR, "Dupl failed %#x", hr);
        return DUPL_RETURN::DUPL_RETURN_ERROR_UNEXPECTED;
    }

    // If still holding old frame, destroy it
    if (m_AcquiredDesktopImage) {
        m_AcquiredDesktopImage = nullptr;
    }

    // QI for IDXGIResource
    hr = DesktopResource->QueryInterface(
        __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(m_AcquiredDesktopImage.GetAddressOf()));
    if (FAILED(hr)) {
        // return ProcessFailure(
        //     nullptr,
        //     L"Failed to QI for ID3D11Texture2D from acquired IDXGIResource in
        //     DUPLICATIONMANAGER", L"Error", hr);
        return DUPL_RETURN::DUPL_RETURN_ERROR_UNEXPECTED;
    }

    // Get metadata
    if (FrameInfo.TotalMetadataBufferSize) {
        // Old buffer too small
        if (FrameInfo.TotalMetadataBufferSize > m_MetaDataSize) {
            if (m_MetaDataBuffer) {
                delete[] m_MetaDataBuffer;
                m_MetaDataBuffer = nullptr;
            }
            m_MetaDataBuffer = new (std::nothrow) BYTE[FrameInfo.TotalMetadataBufferSize];
            if (!m_MetaDataBuffer) {
                m_MetaDataSize = 0;
                Data->MoveCount = 0;
                Data->DirtyCount = 0;
                // return ProcessFailure(
                //     nullptr, L"Failed to allocate memory for metadata in DUPLICATIONMANAGER",
                //     L"Error", E_OUTOFMEMORY);
                return DUPL_RETURN::DUPL_RETURN_ERROR_UNEXPECTED;
            }
            m_MetaDataSize = FrameInfo.TotalMetadataBufferSize;
        }

        UINT BufSize = FrameInfo.TotalMetadataBufferSize;

        // Get move rectangles
        hr = m_DeskDupl->GetFrameMoveRects(
            BufSize, reinterpret_cast<DXGI_OUTDUPL_MOVE_RECT*>(m_MetaDataBuffer), &BufSize);
        if (FAILED(hr)) {
            Data->MoveCount = 0;
            Data->DirtyCount = 0;
            // return ProcessFailure(nullptr, L"Failed to get frame move rects in
            // DUPLICATIONMANAGER",
            //                       L"Error", hr, FrameInfoExpectedErrors);
            LOGF(ERR, "Dupl GetFrameMoveRects failed %#x", hr);
            return DUPL_RETURN::DUPL_RETURN_ERROR_UNEXPECTED;
        }
        Data->MoveCount = BufSize / sizeof(DXGI_OUTDUPL_MOVE_RECT);

        BYTE* DirtyRects = m_MetaDataBuffer + BufSize;
        BufSize = FrameInfo.TotalMetadataBufferSize - BufSize;

        // Get dirty rectangles
        hr = m_DeskDupl->GetFrameDirtyRects(BufSize, reinterpret_cast<RECT*>(DirtyRects), &BufSize);
        if (FAILED(hr)) {
            Data->MoveCount = 0;
            Data->DirtyCount = 0;
            // return ProcessFailure(nullptr, L"Failed to get frame dirty rects in
            // DUPLICATIONMANAGER",
            //                       L"Error", hr, FrameInfoExpectedErrors);
            LOGF(ERR, "Dupl GetFrameDirtyRects failed %#x", hr);
            return DUPL_RETURN::DUPL_RETURN_ERROR_UNEXPECTED;
        }
        Data->DirtyCount = BufSize / sizeof(RECT);

        Data->MetaData = m_MetaDataBuffer;
    }

    Data->Frame = m_AcquiredDesktopImage.Get();
    Data->FrameInfo = FrameInfo;

    return DUPL_RETURN_SUCCESS;
}

WARNING_ENABLE(6101)

//
// Release frame
//
DUPL_RETURN DUPLICATIONMANAGER::DoneWithFrame() {
    if (m_DeskDupl == nullptr) {
        return DUPL_RETURN::DUPL_RETURN_ERROR_UNEXPECTED;
    }
    HRESULT hr = m_DeskDupl->ReleaseFrame();
    if (FAILED(hr)) {
        // return ProcessFailure(m_Device, L"Failed to release frame in DUPLICATIONMANAGER",
        // L"Error",
        //                       hr, FrameInfoExpectedErrors);
        return DUPL_RETURN::DUPL_RETURN_ERROR_UNEXPECTED;
    }

    if (m_AcquiredDesktopImage) {
        m_AcquiredDesktopImage = nullptr;
    }

    return DUPL_RETURN_SUCCESS;
}

//
// Gets output desc into DescPtr
//
void DUPLICATIONMANAGER::GetOutputDesc(_Out_ DXGI_OUTPUT_DESC* DescPtr) {
    *DescPtr = m_OutputDesc;
}

void DUPLICATIONMANAGER::WaitForVBlank() {
    assert(m_DxgiOutput);
    m_DxgiOutput->WaitForVBlank();
}

bool DUPLICATIONMANAGER::DefaultOutput() {
    return default_output_;
}

DUPL_RETURN DUPLICATIONMANAGER::ResetDulp() {
    if (!ltlib::setThreadDesktop()) {
        return DUPL_RETURN_ERROR_UNEXPECTED;
    }
    if (m_DeskDupl) {
        m_DeskDupl = nullptr;
    }
    m_DxgiOutput->GetDesc(&m_OutputDesc);
    ComPtr<IDXGIOutput1> DxgiOutput1;
    HRESULT hr = m_DxgiOutput->QueryInterface(__uuidof(IDXGIOutput1),
                                              reinterpret_cast<void**>(DxgiOutput1.GetAddressOf()));
    if (FAILED(hr)) {
        LOGF(ERR, "Failed to QI for DxgiOutput1 in DUPLICATIONMANAGER, hr: 0x%08x", hr);
        return DUPL_RETURN_ERROR_UNEXPECTED;
    }

    hr = DxgiOutput1->DuplicateOutput(m_Device.Get(), m_DeskDupl.GetAddressOf());
    if (FAILED(hr)) {
        if (hr == DXGI_ERROR_NOT_CURRENTLY_AVAILABLE) {
            LOGF(ERR, "There is already the maximum number of applications using the Desktop "
                      "Duplication API running, please close one of those applications and then "
                      "try again.");
        }
        LOGF(ERR, "failed to call DuplicateOutput, hr:0x%08x", hr);
        return DUPL_RETURN_ERROR_UNEXPECTED;
    }
    return DUPL_RETURN_ERROR_UNEXPECTED;
}
