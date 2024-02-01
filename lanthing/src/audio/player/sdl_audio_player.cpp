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

#include "sdl_audio_player.h"

#include <SDL_audio.h>
#include <ltlib/logging.h>

// 参考
//  https://lazyfoo.net/tutorials/SDL/34_audio_recording/index.php

namespace lt {

namespace audio {

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
        LOG(ERR) << "SDL_OpenAudioDevice failed:" << SDL_GetError();
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

} // namespace audio

} // namespace lt