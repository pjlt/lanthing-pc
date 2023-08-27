#pragma once
#include <cstdint>
#include <memory>
#include <vector>

#include <transport/transport.h>

namespace lt {

class AudioPlayer {
public:
    struct Params {
        AudioCodecType type;
        uint32_t frames_per_second;
        uint32_t channels;
    };

public:
    static std::unique_ptr<AudioPlayer> create(const Params& params);
    virtual ~AudioPlayer();
    void submit(const void* data, uint32_t size);

protected:
    AudioPlayer(const Params& params);
    virtual bool initPlatform() = 0;
    virtual bool play(const void* data, uint32_t size) = 0;
    uint32_t framesPerSec() const;
    uint32_t framesPer10ms() const;
    uint32_t channels() const;

private:
    bool init();
    bool initDecoder();
    bool needDecode() const;
    int32_t decode(const void* data, uint32_t size);

private:
    const AudioCodecType type_;
    void* opus_decoder_ = nullptr;
    uint32_t frames_per_sec_;
    uint32_t channels_;
    std::vector<uint8_t> buffer_;
};

} // namespace lt