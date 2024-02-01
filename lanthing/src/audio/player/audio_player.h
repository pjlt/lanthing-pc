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

#pragma once
#include <cstdint>
#include <memory>
#include <vector>

#include <transport/transport.h>

namespace lt {

namespace audio {

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

} // namespace audio

} // namespace lt