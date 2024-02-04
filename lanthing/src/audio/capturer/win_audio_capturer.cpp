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

#include "win_audio_capturer.h"

#include <Functiondiscoverykeys_devpkey.h>

#include <ltlib/logging.h>

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

class SimpleGuard {
public:
    SimpleGuard(const std::function<void()>& cleanup)
        : cleanup_{cleanup} {}
    ~SimpleGuard() { cleanup_(); }

private:
    std::function<void()> cleanup_;
};
} // namespace

namespace lt {

namespace audio {

using namespace Microsoft::WRL;

WinAudioCapturer::WinAudioCapturer(const Params& params)
    : Capturer{params} {
    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (SUCCEEDED(hr)) {
        need_co_uninit_ = true;
    }
    stop_ev_ = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    read_ev_ = CreateEventW(nullptr, FALSE, FALSE, nullptr);
}

WinAudioCapturer::~WinAudioCapturer() {
    ::SetEvent(stop_ev_);
    {
        std::unique_lock lock{mtx_};
        cv_.wait(lock, [this]() { return !running_; });
    }
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
    if (swr_context_) {
        swr_free(&swr_context_);
    }
}

bool WinAudioCapturer::initPlatform() {
    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL,
                                  __uuidof(IMMDeviceEnumerator),
                                  reinterpret_cast<void**>(enumerator_.GetAddressOf()));
    if (FAILED(hr)) {
        LOG(ERR) << "CoCreateInstance IMMDeviceEnumerator failed with " << toHex(hr);
        return false;
    }
    IMMDevice* mmdevice = nullptr;
    hr = enumerator_->GetDefaultAudioEndpoint(EDataFlow::eRender, ERole::eConsole, &mmdevice);
    if (FAILED(hr)) {
        LOG(ERR) << "IMMDeviceEnumerator::GetDefaultAudioEndpoint failed with " << toHex(hr);
        return false;
    }
    device_ = mmdevice;
    getDeviceName();
    hr = device_->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                           reinterpret_cast<void**>(client_.GetAddressOf()));
    if (FAILED(hr)) {
        LOG(ERR) << "IMMDevice::Activate failed with " << toHex(hr);
        return false;
    }
    printAudioEngineInternalFormat();
    if (!setAudioFormat()) {
        return false;
    }
    if (fmt_to_.has_value()) {
        if (!createResampler()) {
            return false;
        }
    }
    hr = client_->SetEventHandle(read_ev_);
    if (FAILED(hr)) {
        LOG(ERR) << "IAudioClient::SetEventHandle failed with " << toHex(hr);
        return false;
    }
    hr = client_->GetService(__uuidof(IAudioCaptureClient),
                             reinterpret_cast<void**>(capturer_.GetAddressOf()));
    if (FAILED(hr)) {
        LOG(ERR) << "IAudioClient::GetService(IAudioCaptureClient) failed with " << toHex(hr);
        return false;
    }
    hr = client_->GetBufferSize(&buffer_len_); // 注意，单位是帧
    if (FAILED(hr)) {
        LOG(ERR) << "IAudioClient::GetBufferSize failed with " << toHex(hr);
        return false;
    }
    hr = client_->Start();
    if (FAILED(hr)) {
        LOG(ERR) << "IAudioClient::Start failed with " << toHex(hr);
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
    SimpleGuard guard{[this]() {
        {
            std::lock_guard lk{mtx_};
            running_ = false;
        }
        cv_.notify_one();
    }};
    HANDLE events[2] = {stop_ev_, read_ev_};
    HRESULT hr = S_OK;
    while (true) {
        i_am_alive();
        DWORD waitResult = WaitForMultipleObjects(2, events, FALSE, 2000);
        switch (waitResult) {
        case WAIT_OBJECT_0 + 0:
            LOG(INFO) << "Audio capture loop received shutdown event";
            return;
        case WAIT_OBJECT_0 + 1:
            break;
        case WAIT_TIMEOUT:
            LOG(WARNING) << "Capture event timed out after 2 seconds";
            break;
        default:
            LOG(WARNING) << "Unknown wait termination on audio capture";
            return;
        }
        uint32_t next_packet_size = 0;
        hr = capturer_->GetNextPacketSize(&next_packet_size);
        if (FAILED(hr)) {
            LOG(ERR) << "IAudioCaptureClient::GetNextPacketSize failed with " << toHex(hr);
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
                LOG(ERR) << "IAudioCaptureClient::GetBuffer failed with " << toHex(hr);
                return;
            }
            if (AUDCLNT_S_BUFFER_EMPTY == hr) {
                break;
            }
            if (flags & AUDCLNT_BUFFERFLAGS_SILENT) {
                // LOG(DEBUG) << "AUDCLNT_BUFFERFLAGS_SILENT";
                if (slient_buffer_.size() < framesAvailable * fmt_from_->Format.nBlockAlign) {
                    slient_buffer_.resize(framesAvailable * fmt_from_->Format.nBlockAlign);
                    memset(slient_buffer_.data(), 0, slient_buffer_.size());
                }
                pData = slient_buffer_.data();
            }
            if (swr_context_) {
                resample(pData, framesAvailable);
            }
            else {
                onCapturedData(pData, framesAvailable);
            }
            hr = capturer_->ReleaseBuffer(framesAvailable);
            if (FAILED(hr)) {
                LOG(ERR) << "IAudioCaptureClient::ReleaseBuffer failed with " << toHex(hr);
                return;
            }
            hr = capturer_->GetNextPacketSize(&next_packet_size);
            if (FAILED(hr)) {
                LOG(ERR) << "IAudioCaptureClient::GetNextPacketSize failed with " << toHex(hr);
                return;
            }
        }
    }
}

void WinAudioCapturer::getDeviceName() {
    ComPtr<IPropertyStore> store;
    HRESULT hr = device_->OpenPropertyStore(STGM_READ, store.GetAddressOf());
    if (FAILED(hr)) {
        LOG(ERR) << "IMMDevice::OpenPropertyStore failed with " << toHex(hr);
        return;
    }
    PROPVARIANT name{};
    hr = store->GetValue(PKEY_Device_FriendlyName, &name);
    if (FAILED(hr)) {
        LOG(ERR) << "IPropertyStore::GetValue(PKEY_Device_FriendlyName) failed with " << toHex(hr);
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
        LOG(ERR) << "IAudioClient::GetMixFormat failed with " << toHex(hr);
        CoTaskMemFree(wformat);
        return;
    }
    LOGF(INFO,
         "Audio internal format wFormatTag:%#x, nChannels:%u, nSamplesPerSec:%lu, "
         "nAvgBytesPerSec:%lu, nBlockAlign:%u, wBitsPerSample:%u, cbSize:%u",
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
                fmt_from_ = wfmte;
                break;
            }
            else {
                if (pWfxClosestMatch) {
                    if (!fmt_from_.has_value()) {
                        fmt_from_ = WAVEFORMATEXTENSIBLE{};
                        fmt_from_->Format = *pWfxClosestMatch;
                        fmt_from_->dwChannelMask = 0;
                        fmt_from_->Samples.wValidBitsPerSample = pWfxClosestMatch->wBitsPerSample;
                        fmt_from_->SubFormat = KSDATAFORMAT_SUBTYPE_PCM;
                        fmt_to_ = wfmte.Format;
                    }
                    LOG(INFO) << "nChannels=" << wfmte.Format.nChannels
                              << ", nSamplesPerSec=" << wfmte.Format.nSamplesPerSec
                              << ", nBlockAlign=" << wfmte.Format.nBlockAlign
                              << " is not supported. Closest match: "
                                 "nChannels="
                              << pWfxClosestMatch->nChannels
                              << ", nSamplesPerSec=" << pWfxClosestMatch->nSamplesPerSec
                              << ", nBlockAlign=" << pWfxClosestMatch->nBlockAlign;
                    CoTaskMemFree(pWfxClosestMatch);
                    pWfxClosestMatch = NULL;
                }
                else {
                    LOG(INFO) << "nChannels=" << wfmte.Format.nChannels
                              << ", nSamplesPerSec=" << wfmte.Format.nSamplesPerSec
                              << ", nBlockAlign=" << wfmte.Format.nBlockAlign
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
             "Audio capture format: wFormatTag:%#x, nChannels:%u, nSamplesPerSec:%lu, , "
             "nAvgBytesPerSec:%lu, nBlockAlign:%u, wBitsPerSample:%u, cbSize:%u",
             wfmte.Format.wFormatTag, wfmte.Format.nChannels, wfmte.Format.nSamplesPerSec,
             wfmte.Format.nAvgBytesPerSec, wfmte.Format.nBlockAlign, wfmte.Format.wBitsPerSample,
             wfmte.Format.cbSize);
        hr = client_->Initialize(AUDCLNT_SHAREMODE_SHARED,
                                 AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_LOOPBACK,
                                 0, 0, reinterpret_cast<WAVEFORMATEX*>(&wfmte), NULL);
        if (FAILED(hr)) {
            LOG(ERR) << "IAudioClient::Initialize failed with " << toHex(hr);
        }
    }
    else if (fmt_from_.has_value()) {
        setBytesPerFrame(fmt_to_->nBlockAlign);
        setFramesPerSec(fmt_to_->nSamplesPerSec);
        setChannels(fmt_to_->nChannels);
        LOGF(WARNING,
             "Using first closest audio capture format: wFormatTag:%#x, nChannels:%u, "
             "nSamplesPerSec:%lu, nAvgBytesPerSec:%lu, nBlockAlign:%u, "
             "wBitsPerSample:%u, cbSize:%u",
             fmt_from_->Format.wFormatTag, fmt_from_->Format.nChannels,
             fmt_from_->Format.nSamplesPerSec, fmt_from_->Format.nAvgBytesPerSec,
             fmt_from_->Format.nBlockAlign, fmt_from_->Format.wBitsPerSample,
             fmt_from_->Format.cbSize);
        auto format = fmt_from_.value();
        hr = client_->Initialize(AUDCLNT_SHAREMODE_SHARED,
                                 AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_LOOPBACK,
                                 0, 0, reinterpret_cast<WAVEFORMATEX*>(&format), NULL);
        if (FAILED(hr)) {
            LOG(ERR) << "IAudioClient::Initialize failed with " << toHex(hr);
        }
    }
    CoTaskMemFree(pWfxClosestMatch);
    return hr == S_OK;
}

bool WinAudioCapturer::createResampler() {
    constexpr size_t kBuffLen = 1024;
    char strbuff[kBuffLen] = {0};
    int input_ch = fmt_from_->Format.nChannels;
    int output_ch = fmt_to_->nChannels;
    AVSampleFormat input_format;
    switch (fmt_from_->Format.wBitsPerSample) {
    case 16:
        input_format = AVSampleFormat::AV_SAMPLE_FMT_S16;
        break;
    case 32:
        input_format = AVSampleFormat::AV_SAMPLE_FMT_S32;
        break;
    default:
        return false;
    }
    AVSampleFormat output_format = AVSampleFormat::AV_SAMPLE_FMT_S16;
    av_channel_layout_default(&input_ch_layout_, input_ch);
    av_channel_layout_default(&output_ch_layout_, output_ch);
    int ret = swr_alloc_set_opts2(&swr_context_, &output_ch_layout_, output_format,
                                  fmt_to_->nSamplesPerSec, &input_ch_layout_, input_format,
                                  fmt_from_->Format.nSamplesPerSec, 0, nullptr);
    if (swr_context_ == nullptr) {
        int ret2 = av_strerror(ret, strbuff, kBuffLen);
        LOG(ERR) << "swr_alloc_set_opts2 failed: " << (ret2 == 0 ? strbuff : std::to_string(ret));
        return false;
    }
    ret = swr_init(swr_context_);
    if (ret != 0) {
        int ret2 = av_strerror(ret, strbuff, kBuffLen);
        LOG(ERR) << "swr_init failed: " << (ret2 == 0 ? strbuff : std::to_string(ret));
        return false;
    }
    return true;
}

void WinAudioCapturer::resample(const uint8_t* data, uint32_t frames) {
    int64_t delay = swr_get_delay(swr_context_, fmt_from_->Format.nSamplesPerSec);
    int64_t eframes = av_rescale_rnd(
        delay + static_cast<int64_t>(frames), static_cast<int64_t>(fmt_to_->nSamplesPerSec),
        static_cast<int64_t>(fmt_from_->Format.nSamplesPerSec), AV_ROUND_UP);
    int64_t buff_size = static_cast<int64_t>(resample_buffer_.size());
    int64_t ebuff_size = eframes * fmt_to_->nSamplesPerSec * fmt_to_->nChannels;
    if (ebuff_size > buff_size) {
        resample_buffer_.resize(static_cast<size_t>(ebuff_size));
    }
    auto output_data = resample_buffer_.data();
    int ret = swr_convert(swr_context_, &output_data, eframes, &data, frames);
    if (ret < 0) {
        if (!has_logged_) {
            has_logged_ = true;
            constexpr size_t kBuffLen = 1024;
            char strbuff[kBuffLen] = {0};
            int ret2 = av_strerror(ret, strbuff, kBuffLen);
            LOG(ERR) << "swr_convert failed: " << (ret2 == 0 ? strbuff : std::to_string(ret));
        }
    }
    else {
        onCapturedData(resample_buffer_.data(), static_cast<uint32_t>(ret));
    }
}

} // namespace audio

} // namespace lt