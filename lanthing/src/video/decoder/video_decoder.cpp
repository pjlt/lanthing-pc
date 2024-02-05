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

#include "video_decoder.h"

#include "ffmpeg_hard_decoder.h"
#include "openh264_decoder.h"

namespace lt {

namespace video {

std::unique_ptr<Decoder> Decoder::create(const Params& params) {
    // auto decoder = std::make_unique<FFmpegHardDecoder>(params);
    // if (!decoder->init()) {
    //     return nullptr;
    // }
    auto decoder = std::make_unique<OpenH264Decoder>(params);
    if (!decoder->init()) {
        return nullptr;
    }
    return decoder;
}

uint32_t Decoder::align(lt::VideoCodecType type) {
    // NOTE: 下列值匹配ffmpeg，如果需要支持其它解码器，要改这里
    switch (type) {
    case lt::VideoCodecType::H264_420:
    case lt::VideoCodecType::H264_444:
        return 16;
    case lt::VideoCodecType::H265_420:
    case lt::VideoCodecType::H265_444:
        return 128;
    default:
        return 0;
    }
}

Decoder::Decoder(const Params& params)
    : codec_type_{params.codec_type}
    , width_{params.width}
    , height_{params.height} {}

VideoCodecType Decoder::codecType() const {
    return codec_type_;
}

uint32_t Decoder::width() const {
    return width_;
}

uint32_t Decoder::height() const {
    return height_;
}

} // namespace video

} // namespace lt