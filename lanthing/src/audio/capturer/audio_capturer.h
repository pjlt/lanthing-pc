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

#include <ltlib/threads.h>

#include <transport/transport.h>

namespace lt {

class AudioCapturer {
public:
    struct Params {
        AudioCodecType type;
        std::function<void(const std::shared_ptr<google::protobuf::MessageLite>&)> on_audio_data;
    };

public:
    static std::unique_ptr<AudioCapturer> create(const Params& params);
    virtual ~AudioCapturer();
    void start();
    void stop();
    uint32_t bytesPerFrame() const;
    uint32_t channels() const;
    uint32_t framesPerSec() const;
    uint32_t framesPer10ms() const;
    uint32_t bytesPer10ms() const;

protected:
    AudioCapturer(const Params& params);
    virtual bool initPlatform() = 0;
    virtual void captureLoop(const std::function<void()>& i_am_alive) = 0;
    void onCapturedData(const uint8_t* data, uint32_t frames);

    void setBytesPerFrame(uint32_t value);
    void setChannels(uint32_t value);
    void setFramesPerSec(uint32_t value);

private:
    bool init();
    bool needEncode() const;
    bool initEncoder();

private:
    const AudioCodecType type_;
    std::function<void(const std::shared_ptr<google::protobuf::MessageLite>&)> on_audio_data_;
    std::unique_ptr<ltlib::BlockingThread> capture_thread_;
    std::atomic<bool> stoped_{true};
    uint32_t bytes_per_frame_ = 0;
    uint32_t channels_ = 0;
    uint32_t frames_per_sec_ = 0;
    std::shared_ptr<uint8_t> pcm_buffer_;
    uint32_t pcm_buffer_size_ = 0;
    std::vector<uint8_t> opus_buffer_;
    void* opus_encoder_ = nullptr;
};

} // namespace lt