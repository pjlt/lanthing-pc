#pragma once
#include <cstdint>
#include <functional>
#include <memory>

#include <google/protobuf/message_lite.h>

#include <ltlib/threads.h>

#include <transport/transport.h>

namespace lt {

class AudioCapturer {
public:
    struct Params {
        AudioCodecType type;
        std::function<void(const std::shared_ptr<google::protobuf::MessageLite>&)> on_audio_data;
    };

public:
    static std::unique_ptr<AudioCapturer> create(const Params& params);
    virtual ~AudioCapturer();
    void start();
    void stop();
    uint32_t bytesPerFrame() const;
    uint32_t channels() const;
    uint32_t framesPerSec() const;
    uint32_t framesPer10ms() const;
    uint32_t bytesPer10ms() const;

protected:
    AudioCapturer(const Params& params);
    virtual bool initPlatform() = 0;
    virtual void captureLoop(const std::function<void()>& i_am_alive) = 0;
    void onCapturedData(const uint8_t* data, uint32_t frames);

    void setBytesPerFrame(uint32_t value);
    void setChannels(uint32_t value);
    void setFramesPerSec(uint32_t value);

private:
    bool init();
    bool needEncode() const;
    bool initEncoder();

private:
    const AudioCodecType type_;
    std::function<void(const std::shared_ptr<google::protobuf::MessageLite>&)> on_audio_data_;
    std::unique_ptr<ltlib::BlockingThread> capture_thread_;
    std::atomic<bool> stoped_{true};
    uint32_t bytes_per_frame_ = 0;
    uint32_t channels_ = 0;
    uint32_t frames_per_sec_ = 0;
    std::shared_ptr<uint8_t> pcm_buffer_;
    uint32_t pcm_buffer_size_ = 0;
    std::vector<uint8_t> opus_buffer_;
    void* opus_encoder_ = nullptr;
};

} // namespace lt