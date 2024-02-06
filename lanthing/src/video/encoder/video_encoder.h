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
#include <array>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <vector>

#include <ltproto/client2worker/video_frame.pb.h>

#include <transport/transport.h>
#include <video/capturer/video_capturer.h>

namespace lt {

namespace video {

class Encoder {
public:
    struct InitParams {
        // 其实有一个device就够了，但是在创建d3ddevice的时候就能顺便获取其他值，就干脆传递过来
        int64_t luid;
        uint32_t vendor_id;
        void* device;
        void* context;
        lt::VideoCodecType codec_type = lt::VideoCodecType::H264;
        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t bitrate_bps = 0;
        uint32_t freq = 0;

        bool validate() const;
    };

    struct ReconfigureParams {
        std::optional<uint32_t> bitrate_bps;
        std::optional<uint32_t> fps;
    };

public:
    static std::unique_ptr<Encoder> createHard(const InitParams& params);
    static std::unique_ptr<Encoder> createSoft(const InitParams& params);
    virtual ~Encoder();
    virtual void reconfigure(const ReconfigureParams& params) = 0;
    virtual CaptureFormat captureFormat() const = 0;
    virtual VideoCodecType codecType() const = 0;
    void requestKeyframe();
    std::shared_ptr<ltproto::client2worker::VideoFrame> encode(const Capturer::Frame& input_frame);

    // static std::vector<VideoCodecType> checkSupportedCodecs(uint32_t width, uint32_t height);
    // static std::vector<VideoCodecType> checkSupportedCodecsWithLuid(int64_t luid, uint32_t width,
    //                                                                 uint32_t height);

protected:
    Encoder(void* d3d11_dev, void* d3d11_ctx, uint32_t width, uint32_t height);
    bool needKeyframe();
    virtual std::shared_ptr<ltproto::client2worker::VideoFrame> encodeFrame(void* input_frame) = 0;

private:
    void* d3d11_dev_ = nullptr;
    void* d3d11_ctx_ = nullptr;
    const uint32_t width_;
    const uint32_t height_;
    uint64_t frame_id_ = 0;
    std::atomic<bool> request_keyframe_{false};
    bool first_frame_ = false;
};

} // namespace video

} // namespace lt