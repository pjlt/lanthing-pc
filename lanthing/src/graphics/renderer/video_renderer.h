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
#include <memory>
#include <vector>

namespace lt {

class VideoRenderer {
public:
    struct Params {
        void* window;
        uint64_t device;
        uint32_t video_width;
        uint32_t video_height;
        uint32_t align;
    };

    enum class RenderResult { Success2, Failed, Reset };

public:
    static std::unique_ptr<VideoRenderer> create(const Params& params);
    virtual ~VideoRenderer() = default;
    virtual bool bindTextures(const std::vector<void*>& textures) = 0;
    virtual RenderResult render(int64_t frame) = 0;
    virtual void updateCursor(int32_t cursor_id, float x, float y, bool visible) = 0;
    virtual void switchMouseMode(bool absolute) = 0;
    virtual void resetRenderTarget() = 0;
    virtual bool present() = 0;
    virtual bool waitForPipeline(int64_t max_wait_ms) = 0;
    virtual void* hwDevice() = 0;
    virtual void* hwContext() = 0;
    virtual uint32_t displayWidth() = 0;
    virtual uint32_t displayHeight() = 0;
};

} // namespace lt