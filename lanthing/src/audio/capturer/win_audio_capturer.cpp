#include "win_audio_capturer.h"

#include <Functiondiscoverykeys_devpkey.h>

#include <g3log/g3log.hpp>

// wasapi的用法参考了下面的链接：
//  https://learn.microsoft.com/en-us/windows/win32/api/_coreaudio/
//  https://webrtc.googlesource.com/src/+/refs/heads/main/modules/audio_device/win/audio_device_core_win.cc
//  https://github.com/obsproject/obs-studio/blob/master/plugins/win-wasapi/win-wasapi.cpp

namespace {
std::string toHex(const int i) {
    char buffer[50];
    snprintf(buffer, sizeof(buffer), "%x", i);

    return std::string(buffer);
}
} // namespace

namespace lt {

using namespace Microsoft::WRL;

WinAudioCapturer::WinAudioCapturer(const Params& params)
    : AudioCapturer{params} {
    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (SUCCEEDED(hr)) {
        need_co_uninit_ = true;
    }
    stop_ev_ = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    read_ev_ = CreateEventW(nullptr, FALSE, FALSE, nullptr);
}

WinAudioCapturer::~WinAudioCapturer() {
    if (stop_ev_) {
        CloseHandle(stop_ev_);
        stop_ev_ = nullptr;
    }
    if (read_ev_) {
        CloseHandle(read_ev_);
        read_ev_ = nullptr;
    }
    if (need_co_uninit_) {
        CoUninitialize();
    }
}

bool WinAudioCapturer::initPlatform() {
    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL,
                                  __uuidof(IMMDeviceEnumerator),
                                  reinterpret_cast<void**>(enumerator_.GetAddressOf()));
    if (FAILED(hr)) {
        LOG(WARNING) << "CoCreateInstance IMMDeviceEnumerator failed with " << toHex(hr);
        return false;
    }
    hr = enumerator_->GetDefaultAudioEndpoint(EDataFlow::eRender, ERole::eConsole,
                                              device_.GetAddressOf());
    if (FAILED(hr)) {
        LOG(WARNING) << "IMMDeviceEnumerator::GetDefaultAudioEndpoint failed with " << toHex(hr);
        return false;
    }
    getDeviceName();
    hr = device_->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                           reinterpret_cast<void**>(client_.GetAddressOf()));
    if (FAILED(hr)) {
        LOG(WARNING) << "IMMDevice::Activate failed with " << toHex(hr);
        return false;
    }
    printAudioEngineInternalFormat();
    if (!setAudioFormat()) {
        return false;
    }
    hr = client_->SetEventHandle(read_ev_);
    if (FAILED(hr)) {
        LOG(WARNING) << "IAudioClient::SetEventHandle failed with " << toHex(hr);
        return false;
    }
    hr = client_->GetService(__uuidof(IAudioCaptureClient),
                             reinterpret_cast<void**>(capturer_.GetAddressOf()));
    if (FAILED(hr)) {
        LOG(WARNING) << "IAudioClient::GetService(IAudioCaptureClient) failed with " << toHex(hr);
        return false;
    }
    hr = client_->GetBufferSize(&buffer_len_); // 注意，单位是帧
    if (FAILED(hr)) {
        LOG(WARNING) << "IAudioClient::GetBufferSize failed with " << toHex(hr);
        return false;
    }
    hr = client_->Start();
    if (FAILED(hr)) {
        LOG(WARNING) << "IAudioClient::Start failed with " << toHex(hr);
        return false;
    }
    return true;
}

void WinAudioCapturer::captureLoop(const std::function<void()>& i_am_alive) {
    /*
     * https://learn.microsoft.com/en-us/windows/win32/coreaudio/capturing-a-stream
     * 从IAudioCaptureClient里取数据，MSDN上给出两种方法，这里用第二种方法
     * 1. The client alternately calls GetBuffer and ReleaseBuffer, reading one packet with each
     * pair of calls, until GetBuffer returns AUDCNT_S_BUFFEREMPTY, indicating that the buffer is
     * empty.
     * 2. The client calls GetNextPacketSize before each pair of calls to GetBuffer and
     * ReleaseBuffer until GetNextPacketSize reports a packet size of 0, indicating that the buffer
     * is empty.
     */
    HANDLE events[2] = {stop_ev_, read_ev_};
    HRESULT hr = S_OK;
    while (true) {
        i_am_alive();
        DWORD waitResult = WaitForMultipleObjects(2, events, FALSE, 500);
        switch (waitResult) {
        case WAIT_OBJECT_0 + 0:
            LOG(INFO) << "Audio capture loop received shutdown event";
            return;
        case WAIT_OBJECT_0 + 1:
            break;
        case WAIT_TIMEOUT:
            LOG(WARNING) << "Capture event timed out after 0.5 seconds";
            return;
        default:
            LOG(WARNING) << "Unknown wait termination on audio capture";
            return;
        }
        uint32_t next_packet_size = 0;
        hr = capturer_->GetNextPacketSize(&next_packet_size);
        if (FAILED(hr)) {
            LOG(WARNING) << "IAudioCaptureClient::GetNextPacketSize failed with " << toHex(hr);
            return;
        }
        while (next_packet_size != 0) {
            // https://sound.stackexchange.com/questions/41567/difference-between-frame-and-sample-in-waveform
            // 在音频领域似乎没有对'帧'的定义，但是在许多音频技术的上下文里，一直有'帧'的身影
            // 需要注意的是，在不同语境下，'帧'的含义不一样
            // 下面的'帧'，包括成员变量'buffer_len_'指'采样次数'
            BYTE* pData = 0;
            UINT32 framesAvailable = 0;
            DWORD flags = 0;
            UINT64 recTime = 0;
            UINT64 recPos = 0;
            hr = capturer_->GetBuffer(&pData, &framesAvailable, &flags, &recPos, &recTime);
            if (FAILED(hr)) {
                LOG(WARNING) << "IAudioCaptureClient::GetBuffer failed with " << toHex(hr);
                return;
            }
            if (AUDCLNT_S_BUFFER_EMPTY == hr) {
                break;
            }
            if (flags & AUDCLNT_BUFFERFLAGS_SILENT) {
                // LOG(DEBUG) << "AUDCLNT_BUFFERFLAGS_SILENT";
                pData = nullptr;
            }
            onCapturedData(pData, framesAvailable);
            hr = capturer_->ReleaseBuffer(framesAvailable);
            if (FAILED(hr)) {
                LOG(WARNING) << "IAudioCaptureClient::ReleaseBuffer failed with " << toHex(hr);
                return;
            }
            hr = capturer_->GetNextPacketSize(&next_packet_size);
            if (FAILED(hr)) {
                LOG(WARNING) << "IAudioCaptureClient::GetNextPacketSize failed with " << toHex(hr);
                return;
            }
        }
    }
}

void WinAudioCapturer::getDeviceName() {
    ComPtr<IPropertyStore> store;
    HRESULT hr = device_->OpenPropertyStore(STGM_READ, store.GetAddressOf());
    if (FAILED(hr)) {
        LOG(WARNING) << "IMMDevice::OpenPropertyStore failed with " << toHex(hr);
        return;
    }
    PROPVARIANT name{};
    hr = store->GetValue(PKEY_Device_FriendlyName, &name);
    if (FAILED(hr)) {
        LOG(WARNING) << "IPropertyStore::GetValue(PKEY_Device_FriendlyName) failed with "
                     << toHex(hr);
        return;
    }
    if (name.pszVal && *name.pszVal) {
        device_name_ = name.pszVal;
        LOG(INFO) << "Using audio device " << name.pszVal;
    }
}

void WinAudioCapturer::printAudioEngineInternalFormat() {
    WAVEFORMATEX* wformat = nullptr;
    HRESULT hr = client_->GetMixFormat(&wformat);
    if (FAILED(hr)) {
        LOG(WARNING) << "IAudioClient::GetMixFormat failed with " << toHex(hr);
        CoTaskMemFree(wformat);
        return;
    }
    LOGF(INFO,
         "Audio internal format wFormatTag:%#x, nChannels:%d, nSamplesPerSec:%d, "
         "nAvgBytesPerSec:%d, nBlockAlign:%d, wBitsPerSample:%d, cbSize:%d",
         wformat->wFormatTag, wformat->nChannels, wformat->nSamplesPerSec, wformat->nAvgBytesPerSec,
         wformat->nBlockAlign, wformat->wBitsPerSample, wformat->cbSize);
    CoTaskMemFree(wformat);
}

bool WinAudioCapturer::setAudioFormat() {
    // 目前用到的RTC通道基于WebRTC二次开发，而WebRTC对音频格式有一定要求，所以下面关于音频格式的代码源自WebRTC
    WAVEFORMATEXTENSIBLE wfmte{};
    WAVEFORMATEX* pWfxClosestMatch = nullptr;
    wfmte.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
    wfmte.Format.wBitsPerSample = 16;
    wfmte.Format.cbSize = 22;
    wfmte.dwChannelMask = 0;
    wfmte.Samples.wValidBitsPerSample = wfmte.Format.wBitsPerSample;
    wfmte.SubFormat = KSDATAFORMAT_SUBTYPE_PCM;
    const int freqs[6] = {48000, 44100, 16000, 96000, 32000, 8000};
    const WORD prios[3] = {2, 1, 4};
    HRESULT hr = S_FALSE;
    for (unsigned int freq = 0; freq < sizeof(freqs) / sizeof(freqs[0]); freq++) {
        for (unsigned int chan = 0; chan < sizeof(prios) / sizeof(prios[0]); chan++) {
            wfmte.Format.nChannels = prios[chan];
            wfmte.Format.nSamplesPerSec = freqs[freq];
            wfmte.Format.nBlockAlign = wfmte.Format.nChannels * wfmte.Format.wBitsPerSample / 8;
            wfmte.Format.nAvgBytesPerSec = wfmte.Format.nSamplesPerSec * wfmte.Format.nBlockAlign;
            // If the method succeeds and the audio endpoint device supports the
            // specified stream format, it returns S_OK. If the method succeeds and
            // provides a closest match to the specified format, it returns S_FALSE.
            hr = client_->IsFormatSupported(AUDCLNT_SHAREMODE_SHARED, (WAVEFORMATEX*)&wfmte,
                                            &pWfxClosestMatch);
            if (hr == S_OK) {
                break;
            }
            else {
                if (pWfxClosestMatch) {
                    LOG(INFO) << "nChannels=" << wfmte.Format.nChannels
                              << ", nSamplesPerSec=" << wfmte.Format.nSamplesPerSec
                              << " is not supported. Closest match: "
                                 "nChannels="
                              << pWfxClosestMatch->nChannels
                              << ", nSamplesPerSec=" << pWfxClosestMatch->nSamplesPerSec;
                    CoTaskMemFree(pWfxClosestMatch);
                    pWfxClosestMatch = NULL;
                }
                else {
                    LOG(INFO) << "nChannels=" << wfmte.Format.nChannels
                              << ", nSamplesPerSec=" << wfmte.Format.nSamplesPerSec
                              << " is not supported. No closest match.";
                }
            }
        }
        if (hr == S_OK)
            break;
    }
    if (hr == S_OK) {
        setBytesPerFrame(wfmte.Format.nBlockAlign);
        setFramesPerSec(wfmte.Format.nSamplesPerSec);
        setChannels(wfmte.Format.nChannels);
        LOGF(INFO,
             "Audio capture format: wFormatTag:%#x, nChannels:%d, nSamplesPerSec:%d, , "
             "nAvgBytesPerSec:%d, nBlockAlign:%d, wBitsPerSample:%d, cbSize:%d",
             wfmte.Format.wFormatTag, wfmte.Format.nChannels, wfmte.Format.nSamplesPerSec,
             wfmte.Format.nAvgBytesPerSec, wfmte.Format.nBlockAlign, wfmte.Format.wBitsPerSample,
             wfmte.Format.cbSize);
        hr = client_->Initialize(AUDCLNT_SHAREMODE_SHARED,
                                 AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_LOOPBACK,
                                 0, 0, reinterpret_cast<WAVEFORMATEX*>(&wfmte), NULL);
        if (FAILED(hr)) {
            LOG(WARNING) << "IAudioClient::Initialize failed with " << toHex(hr);
        }
    }
    CoTaskMemFree(pWfxClosestMatch);
    return hr == S_OK;
}

} // namespace lt