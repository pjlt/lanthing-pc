#pragma once
#include <audio/player/audio_player.h>

namespace lt {

class SdlAudioPlayer : public AudioPlayer {
public:
    SdlAudioPlayer(const Params& params);
    ~SdlAudioPlayer() override;
    bool initPlatform() override;
    bool play(const void* data, uint32_t size) override;

private:
    uint32_t device_id_ = std::numeric_limits<uint32_t>::max();
};

} // namespace lt