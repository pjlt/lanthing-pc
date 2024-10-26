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
#include <optional>
#include <vector>

#include <cursor_info.h>
#include <video/decoder/video_decoder.h>

namespace lt {

namespace video {

class Renderer {
public:
    struct Params {
        void* window;
        void* device;
        void* context;
        uint32_t video_width;
        uint32_t video_height;
        uint32_t align;
        uint32_t rotation;
        bool stretch;
        bool absolute_mouse;
    };

    enum class RenderResult { Success2, Failed, Reset };

public:
    static std::unique_ptr<Renderer> create(const Params& params);
    virtual ~Renderer() = default;
    // NOTE: bindTextures之后不允许调用setDecodedFormat
    virtual bool bindTextures(const std::vector<void*>& textures) = 0;
    virtual RenderResult render(int64_t frame) = 0;
    void updateCursor(const std::optional<lt::CursorInfo>& cursor_info);
    void switchMouseMode(bool absolute);
    virtual void switchStretchMode(bool stretch) = 0;
    virtual void resetRenderTarget() = 0;
    virtual bool present() = 0;
    virtual bool waitForPipeline(int64_t max_wait_ms) = 0;
    virtual void* hwDevice() = 0;
    virtual void* hwContext() = 0;
    virtual uint32_t displayWidth() = 0;
    virtual uint32_t displayHeight() = 0;
    // NOTE: bindTextures之后不允许调用setDecodedFormat
    virtual bool setDecodedFormat(DecodedFormat format) = 0;
    virtual bool attachRenderContext();
    virtual bool detachRenderContext();

protected:
    explicit Renderer(bool absolute_mouse);

protected:
    std::optional<lt::CursorInfo> cursor_info_;
    bool absolute_mouse_;
};

} // namespace video

} // namespace lt