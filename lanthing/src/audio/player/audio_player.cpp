#include "audio_player.h"
#include "sdl_audio_player.h"

#include <fstream>

#include <g3log/g3log.hpp>
#include <opus/opus.h>

namespace lt {

std::unique_ptr<AudioPlayer> AudioPlayer::create(const Params& params) {
    std::unique_ptr<AudioPlayer> player{new SdlAudioPlayer{params}};
    if (!player->init()) {
        return nullptr;
    }
    return player;
}

AudioPlayer::~AudioPlayer() {
    if (opus_decoder_) {
        auto decoder = reinterpret_cast<OpusDecoder*>(opus_decoder_);
        opus_decoder_destroy(decoder);
        opus_decoder_ = nullptr;
    }
}

AudioPlayer::AudioPlayer(const Params& params)
    : type_{params.type}
    , frames_per_sec_{params.frames_per_second}
    , channels_{params.channels} {
    buffer_.resize(framesPer10ms() * channels() * 2);
}

bool AudioPlayer::init() {
    if (!initDecoder()) {
        return false;
    }
    if (!initPlatform()) {
        return false;
    }
    return true;
}

bool AudioPlayer::initDecoder() {
    if (!needDecode()) {
        return true;
    }
    int error = 0;
    OpusDecoder* decoder = opus_decoder_create(framesPerSec(), channels(), &error);
    if (decoder == nullptr || error < 0) {
        LOG(WARNING) << "opus_decoder_create failed with " << error;
        return false;
    }
    opus_decoder_ = decoder;
    return true;
}

void AudioPlayer::submit(const void* data, uint32_t size) {
    if (needDecode()) {
        int32_t decoded_bytes = decode(data, size);
        if (decoded_bytes > 0) {
            play(buffer_.data(), decoded_bytes);
        }
    }
    else {
        //static std::ofstream out{"./audio_dst", std::ios::binary | std::ios::trunc};
        //out.write(reinterpret_cast<const char*>(data), size);
        //out.flush();
        play(data, size);
    }
}

bool AudioPlayer::needDecode() const {
    return type_ == AudioCodecType::OPUS;
}

int32_t AudioPlayer::decode(const void* data, uint32_t input_size) {
    // static std::ofstream out{"./audio_dst", std::ios::binary | std::ios::trunc};
    // out.write(reinterpret_cast<const char*>(data), input_size);
    // out.flush();

    auto decoder = reinterpret_cast<OpusDecoder*>(opus_decoder_);
    auto input = reinterpret_cast<const unsigned char*>(data);
    auto output = reinterpret_cast<opus_int16*>(buffer_.data());
    auto output_capacity = static_cast<int>(buffer_.size());
    int frames = opus_decode(decoder, input, input_size, output, output_capacity, 0);
    if (frames < 0) {
        LOG(WARNING) << "opus_decode failed with " << frames;
        return frames;
    }
    return frames * channels() * sizeof(opus_int16);
}

uint32_t AudioPlayer::framesPerSec() const {
    return frames_per_sec_;
}

uint32_t AudioPlayer::framesPer10ms() const {
    return framesPerSec() / 100;
}

uint32_t AudioPlayer::channels() const {
    return channels_;
}

} // namespace lt