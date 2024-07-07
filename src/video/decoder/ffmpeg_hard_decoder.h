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
#include <video/decoder/video_decoder.h>

#include <list>
#include <memory>

#include <video/types.h>

namespace lt {

namespace video {

class FFmpegHardDecoder : public Decoder {
public:
    FFmpegHardDecoder(const Params& params);
    ~FFmpegHardDecoder() override;

    bool init();
    DecodedFrame decode(const uint8_t* data, uint32_t size) override;
    std::vector<void*> textures() override;
    DecodedFormat decodedFormat() const override;
    int32_t getHwPixFormat() const;
    void* getHwFrameCtx();

private:
    bool init2(const void* config, const void* codec);
    bool allocatePacketAndFrames();
    bool addRefHwDevCtx();
    void deRefHwDevCtx();

private:
    void* hw_dev_;
    void* hw_ctx_;
    const VaType va_type_;
    void* codec_ctx_ = nullptr;
    void* av_packet_ = nullptr;
    void* av_frame_ = nullptr;
    void* hw_frames_ctx_ = nullptr;
    void* av_hw_ctx_ = nullptr;
    int32_t hw_pix_format_ = -1;
    std::vector<void*> textures_;
};

} // namespace video

} // namespace lt