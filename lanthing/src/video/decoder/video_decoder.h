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

#include <transport/transport.h>
#include <video/types.h>

namespace lt {

namespace video {

// Linux下不知道那个库似乎把'Success'变成常量
enum class DecodeStatus { Success2, EAgain, Failed, NeedReset };

struct DecodedFrame {
    DecodeStatus status;
    int64_t frame;
};

class Decoder {
public:
    struct Params {
        VideoCodecType codec_type;
        uint32_t width;
        uint32_t height;
        void* hw_device;
        void* hw_context;
        VaType va_type;
    };

public:
    static std::unique_ptr<Decoder> create(const Params& params);
    static uint32_t align(lt::VideoCodecType type);
    Decoder(const Params& params);
    virtual ~Decoder() = default;
    virtual DecodedFrame decode(const uint8_t* data, uint32_t size) = 0;
    virtual std::vector<void*> textures() = 0;

    VideoCodecType codecType() const;
    uint32_t width() const;
    uint32_t height() const;

private:
    const VideoCodecType codec_type_;
    const uint32_t width_;
    const uint32_t height_;
};

} // namespace video

} // namespace lt