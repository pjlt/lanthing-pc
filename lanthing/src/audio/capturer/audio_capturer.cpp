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

#include "audio_capturer.h"

#include <fstream>

#include <g3log/g3log.hpp>
#include <opus/opus.h>
#include <opus/opus_types.h>

#include <ltproto/ltproto.h>
#include <ltproto/peer2peer/audio_data.pb.h>

#include "win_audio_capturer.h"

namespace lt {

std::unique_ptr<AudioCapturer> AudioCapturer::create(const Params& params) {
#if LT_WINDOWS
    std::unique_ptr<AudioCapturer> capturer{new WinAudioCapturer{params}};
    if (!capturer->init()) {
        return nullptr;
    }
    return capturer;
#else
    return nullptr;
#endif
}

AudioCapturer::~AudioCapturer() {
    if (opus_encoder_) {
        auto encoder = reinterpret_cast<OpusEncoder*>(opus_encoder_);
        opus_encoder_destroy(encoder);
        opus_encoder_ = nullptr;
    }
}

void AudioCapturer::start() {
    stoped_ = false;
    capture_thread_ = ltlib::BlockingThread::create(
        "audio_capture",
        [this](const std::function<void()>& i_am_alive, void*) { captureLoop(i_am_alive); },
        nullptr);
}

void AudioCapturer::stop() {
    stoped_ = true;
}

AudioCapturer::AudioCapturer(const Params& params)
    : type_{params.type}
    , on_audio_data_{params.on_audio_data} {}

bool AudioCapturer::init() {
    if (!initPlatform()) {
        return false;
    }
    if (!initEncoder()) {
        return false;
    }
    return true;
}

bool AudioCapturer::needEncode() const {
    return type_ == AudioCodecType::OPUS;
}

bool AudioCapturer::initEncoder() {
    if (type_ != AudioCodecType::OPUS) {
        LOG(INFO) << "No need OPUS";
        return true;
    }
    int error = 0;
    OpusEncoder* encoder =
        opus_encoder_create(framesPerSec(), channels(), OPUS_APPLICATION_AUDIO, &error);
    if (encoder == nullptr || error != OPUS_OK) {
        LOG(WARNING) << "opus_encoder_create failed with " << error;
        return false;
    }
    opus_encoder_ctl(encoder, OPUS_SET_BITRATE(framesPerSec() * bytesPerFrame() * 8));
    LOGF(INFO, "OPUS encoder created. fs:%u, channels:%u, bitrate:%u", framesPerSec(), channels(),
         framesPerSec() * bytesPerFrame() * 8);
    opus_buffer_.resize(framesPer10ms() * bytesPerFrame());
    opus_encoder_ = encoder;
    return true;
}

void AudioCapturer::onCapturedData(const uint8_t* data, uint32_t frames) {
    if (data == nullptr) {
        return;
    }
    // static std::ofstream out1{"./audio_pcm", std::ios::binary | std::ios::trunc};
    // out1.write(reinterpret_cast<const char*>(data), frames * bytesPerFrame());
    // out1.flush();
    std::shared_ptr<uint8_t> first_fragment;
    std::vector<const uint8_t*> fragments;
    uint32_t total_size = frames * bytesPerFrame();
    uint32_t index = 0;
    const uint32_t bytes_per_10ms = bytesPer10ms();
    if (pcm_buffer_) {
        uint32_t bytes_need = bytes_per_10ms - pcm_buffer_size_;
        if (bytes_need <= total_size) {
            memcpy(pcm_buffer_.get() + pcm_buffer_size_, data, bytes_need);
            index -= bytes_need;
            first_fragment = std::move(pcm_buffer_);
            fragments.push_back(first_fragment.get());
        }
        else {
            memcpy(pcm_buffer_.get() + pcm_buffer_size_, data, total_size);
            pcm_buffer_size_ += total_size;
            return;
        }
    }
    for (index; index < total_size; index += bytes_per_10ms) {
        if (index + bytes_per_10ms >= total_size) {
            break;
        }
        fragments.push_back(data + index);
    }
    if (index < total_size) {
        pcm_buffer_ = std::shared_ptr<uint8_t>{new uint8_t[bytes_per_10ms]};
        memcpy(pcm_buffer_.get(), data + index, total_size - index);
        pcm_buffer_size_ = total_size - index;
    }

    if (needEncode()) {
        auto encoder = reinterpret_cast<OpusEncoder*>(opus_encoder_);
        for (auto& fragment : fragments) {
            auto pcm_data = reinterpret_cast<const opus_int16*>(fragment);
            auto len = opus_encode(encoder, pcm_data, framesPer10ms(), opus_buffer_.data(),
                                   static_cast<opus_int32>(opus_buffer_.size()));
            if (len < 0) {
                LOG(WARNING) << "opus_encode failed with " << len;
                return;
            }
            auto msg = std::make_shared<ltproto::peer2peer::AudioData>();
            msg->set_data(opus_buffer_.data(), len);
            on_audio_data_(msg);
            // static std::ofstream out{"./audio_src", std::ios::binary | std::ios::trunc};
            // out.write(reinterpret_cast<const char*>(opus_buffer_.data()), len);
            // out.flush();
        }
    }
    else {
        for (auto& fragment : fragments) {
            auto msg = std::make_shared<ltproto::peer2peer::AudioData>();
            msg->set_data(fragment, bytes_per_10ms);
            on_audio_data_(msg);
        }
    }
}

void AudioCapturer::setBytesPerFrame(uint32_t value) {
    bytes_per_frame_ = value;
}

void AudioCapturer::setChannels(uint32_t value) {
    channels_ = value;
}

void AudioCapturer::setFramesPerSec(uint32_t value) {
    frames_per_sec_ = value;
}

uint32_t AudioCapturer::bytesPerFrame() const {
    return bytes_per_frame_;
}

uint32_t AudioCapturer::channels() const {
    return channels_;
}

uint32_t AudioCapturer::framesPerSec() const {
    return frames_per_sec_;
}

uint32_t AudioCapturer::framesPer10ms() const {
    return frames_per_sec_ / 100;
}

uint32_t AudioCapturer::bytesPer10ms() const {
    return bytesPerFrame() * framesPer10ms();
}

} // namespace lt