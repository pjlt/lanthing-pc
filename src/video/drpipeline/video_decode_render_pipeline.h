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
#include <functional>
#include <memory>

#include <google/protobuf/message_lite.h>

#include <cursor_info.h>
#include <plat/pc_sdl.h>
#include <transport/transport.h>

namespace lt {

namespace video {

class DecodeRenderPipeline {
public:
    struct Params {
        Params(lt::VideoCodecType encode, lt::VideoCodecType decode, uint32_t _width,
               uint32_t _height, uint32_t _screen_refresh_rate, uint32_t _rotation, bool _stretch,
               std::function<void(uint32_t, std::shared_ptr<google::protobuf::MessageLite>, bool)>
                   send_message,
               std::function<void()> switch_stretch, std::function<void()> reset_pipeline);
        bool validate() const;

        bool for_test = false;
        lt::VideoCodecType encode_codec;
        lt::VideoCodecType decode_codec;
        uint32_t width;
        uint32_t height;
        uint32_t screen_refresh_rate;
        uint32_t rotation;
        bool stretch;
        bool absolute_mouse;
        bool show_overlay;
        int64_t status_color;
        lt::plat::PcSdl* sdl = nullptr;
        void* device = nullptr;
        void* context = nullptr;
        std::function<void(uint32_t, std::shared_ptr<google::protobuf::MessageLite>, bool)>
            send_message_to_host;
        std::function<void()> switch_stretch;
        std::function<void()> reset_pipeline;
    };

    enum class Action {
        REQUEST_KEY_FRAME = 1,
        NONE = 2,
    };

public:
    static std::unique_ptr<DecodeRenderPipeline> create(const Params& params);
    virtual ~DecodeRenderPipeline() = default;
    virtual Action submit(const lt::VideoFrame& frame) = 0;
    virtual void resetRenderTarget() = 0;
    virtual void setTimeDiff(int64_t diff_us) = 0;
    virtual void setRTT(int64_t rtt_us) = 0;
    virtual void setBWE(uint32_t bps) = 0;
    virtual void setNack(uint32_t nack) = 0;
    virtual void setLossRate(float rate) = 0;
    virtual void setCursorInfo(const ::lt::CursorInfo& info) = 0;
    virtual void switchMouseMode(bool absolute) = 0;
    virtual void switchStretchMode(bool stretch) = 0;

protected:
    DecodeRenderPipeline() = default;
};

} // namespace video

} // namespace lt