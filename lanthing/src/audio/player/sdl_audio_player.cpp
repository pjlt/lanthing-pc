#include "sdl_audio_player.h"

#include <SDL_audio.h>
#include <g3log/g3log.hpp>

// 参考
//  https://lazyfoo.net/tutorials/SDL/34_audio_recording/index.php

namespace lt {

SdlAudioPlayer::SdlAudioPlayer(const Params& params)
    : AudioPlayer{params} {}

SdlAudioPlayer::~SdlAudioPlayer() {
    if (device_id_ != std::numeric_limits<uint32_t>::max()) {
        SDL_PauseAudioDevice(device_id_, 1);
        SDL_CloseAudioDevice(device_id_);
        device_id_ = std::numeric_limits<uint32_t>::max();
    }
}

bool SdlAudioPlayer::initPlatform() {
    SDL_AudioSpec desired{};
    SDL_AudioSpec obtained{};
    desired.freq = framesPerSec();
    desired.format = AUDIO_S16;
    desired.channels = static_cast<Uint8>(channels());
    desired.samples = 4096;

    SDL_AudioDeviceID device_id = SDL_OpenAudioDevice(nullptr, SDL_FALSE, &desired, &obtained, 0);
    if (device_id == 0) {
        LOG(WARNING) << "SDL_OpenAudioDevice failed:" << SDL_GetError();
        return false;
    }
    SDL_PauseAudioDevice(device_id, 0);
    device_id_ = device_id;
    return true;
}

bool SdlAudioPlayer::play(const void* data, uint32_t size) {
    int ret = SDL_QueueAudio(device_id_, data, size);
    if (ret != 0) {
        // 这个错误太多了，注释掉
        // LOG(WARNING) << "SDL_QueueAudio faield:" << SDL_GetError();
        return false;
    }
    return true;
}

} // namespace lt