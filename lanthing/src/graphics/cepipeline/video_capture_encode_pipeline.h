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
#include <functional>
#include <memory>

#include <google/protobuf/message_lite.h>

#include <ltlib/system.h>
#include <message_handler.h>
#include <transport/transport.h>

namespace lt {

class VCEPipeline;
class VideoCaptureEncodePipeline {
public:
    struct Params {
        std::vector<VideoCodecType> codecs;
        uint32_t width;
        uint32_t height;
        ltlib::Monitor monitor;
        std::function<bool(uint32_t, const MessageHandler&)> register_message_handler;
        std::function<bool(uint32_t, const std::shared_ptr<google::protobuf::MessageLite>&)>
            send_message;
    };

public:
    static std::unique_ptr<VideoCaptureEncodePipeline> create(const Params& params);
    bool start();
    void stop();
    VideoCodecType codec() const;
    bool defaultOutput();

private:
    VideoCaptureEncodePipeline() = default;

private:
    std::shared_ptr<VCEPipeline> impl_;
};

} // namespace lt