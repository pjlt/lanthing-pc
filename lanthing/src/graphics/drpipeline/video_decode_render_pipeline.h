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

#include <platforms/pc_sdl.h>
#include <transport/transport.h>

namespace lt {

class VDRPipeline;
class VideoDecodeRenderPipeline {
public:
    struct Params {
        Params(lt::VideoCodecType _codec_type, uint32_t _width, uint32_t _height,
               uint32_t _screen_refresh_rate,
               std::function<void(uint32_t, std::shared_ptr<google::protobuf::MessageLite>, bool)>
                   send_message);
        bool validate() const;

        lt::VideoCodecType codec_type;
        uint32_t width;
        uint32_t height;
        uint32_t screen_refresh_rate;
        PcSdl* sdl = nullptr;
        std::function<void(uint32_t, std::shared_ptr<google::protobuf::MessageLite>, bool)>
            send_message_to_host;
    };

    enum class Action {
        REQUEST_KEY_FRAME = 1,
        NONE = 2,
    };

public:
    static std::unique_ptr<VideoDecodeRenderPipeline> create(const Params& params);
    Action submit(const lt::VideoFrame& frame);
    void resetRenderTarget();
    void setTimeDiff(int64_t diff_us);
    void setRTT(int64_t rtt_us);
    void setBWE(uint32_t bps);
    void setNack(uint32_t nack);
    void setLossRate(float rate);
    void setCursorInfo(int32_t cursor_id, float x, float y, bool visible);
    void switchMouseMode(bool absolute);

private:
    VideoDecodeRenderPipeline() = default;

private:
    std::shared_ptr<VDRPipeline> impl_;
};

} // namespace lt