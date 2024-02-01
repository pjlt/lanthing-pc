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

#include "audio_player.h"
#include "sdl_audio_player.h"

#include <fstream>

#include <ltlib/logging.h>
#include <opus/opus.h>

namespace lt {

namespace audio {

std::unique_ptr<Player> Player::create(const Params& params) {
    std::unique_ptr<Player> player{new SdlAudioPlayer{params}};
    if (!player->init()) {
        return nullptr;
    }
    return player;
}

Player::~Player() {
    if (opus_decoder_) {
        auto decoder = reinterpret_cast<OpusDecoder*>(opus_decoder_);
        opus_decoder_destroy(decoder);
        opus_decoder_ = nullptr;
    }
}

Player::Player(const Params& params)
    : type_{params.type}
    , frames_per_sec_{params.frames_per_second}
    , channels_{params.channels} {
    buffer_.resize(framesPer10ms() * channels() * 2);
}

bool Player::init() {
    if (!initDecoder()) {
        return false;
    }
    if (!initPlatform()) {
        return false;
    }
    return true;
}

bool Player::initDecoder() {
    if (!needDecode()) {
        return true;
    }
    int error = 0;
    OpusDecoder* decoder = opus_decoder_create(framesPerSec(), channels(), &error);
    if (decoder == nullptr || error < 0) {
        LOG(ERR) << "opus_decoder_create failed with " << error;
        return false;
    }
    opus_decoder_ = decoder;
    return true;
}

void Player::submit(const void* data, uint32_t size) {
    if (needDecode()) {
        int32_t decoded_bytes = decode(data, size);
        if (decoded_bytes > 0) {
            play(buffer_.data(), decoded_bytes);
        }
    }
    else {
        // static std::ofstream out{"./audio_dst", std::ios::binary | std::ios::trunc};
        // out.write(reinterpret_cast<const char*>(data), size);
        // out.flush();
        play(data, size);
    }
}

bool Player::needDecode() const {
    return type_ == AudioCodecType::OPUS;
}

int32_t Player::decode(const void* data, uint32_t input_size) {
    // static std::ofstream out{"./audio_dst", std::ios::binary | std::ios::trunc};
    // out.write(reinterpret_cast<const char*>(data), input_size);
    // out.flush();

    auto decoder = reinterpret_cast<OpusDecoder*>(opus_decoder_);
    auto input = reinterpret_cast<const unsigned char*>(data);
    auto output = reinterpret_cast<opus_int16*>(buffer_.data());
    auto output_capacity = static_cast<int>(buffer_.size());
    int frames = opus_decode(decoder, input, input_size, output, output_capacity, 0);
    if (frames < 0) {
        LOG(ERR) << "opus_decode failed with " << frames;
        return frames;
    }
    return frames * channels() * sizeof(opus_int16);
}

uint32_t Player::framesPerSec() const {
    return frames_per_sec_;
}

uint32_t Player::framesPer10ms() const {
    return framesPerSec() / 100;
}

uint32_t Player::channels() const {
    return channels_;
}

} // namespace audio

} // namespace lt