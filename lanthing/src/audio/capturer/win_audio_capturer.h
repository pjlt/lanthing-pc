#pragma once
#include <audio/capturer/audio_capturer.h>

#include <Audioclient.h>
#include <mmdeviceapi.h>
#include <wrl/client.h>

namespace lt {

class WinAudioCapturer : public AudioCapturer {
public:
    WinAudioCapturer(const Params& params);
    ~WinAudioCapturer() override;

    bool initPlatform() override;
    void captureLoop(const std::function<void()>& i_am_alive) override;

private:
    void getDeviceName();
    void printAudioEngineInternalFormat();
    bool setAudioFormat();

private:
    Microsoft::WRL::ComPtr<IMMDeviceEnumerator> enumerator_;
    Microsoft::WRL::ComPtr<IMMDevice> device_;
    Microsoft::WRL::ComPtr<IAudioClient> client_;
    Microsoft::WRL::ComPtr<IAudioCaptureClient> capturer_;
    uint32_t buffer_len_ = 0;
    uint32_t sample_rate_ = 0;
    uint32_t block_size_ = 0;
    uint32_t channels_ = 0;
    std::string device_name_;
    HANDLE read_ev_ = nullptr;
    HANDLE stop_ev_ = nullptr;
    bool need_co_uninit_ = false;
    std::condition_variable cv_;
    std::mutex mtx_;
    bool running_ = false;
};

} // namespace lt