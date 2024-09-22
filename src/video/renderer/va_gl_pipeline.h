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
#include <video/renderer/video_renderer.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GL/gl.h>
#include <GL/glext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <va/va.h>

#include <SDL.h>

namespace lt {

namespace video {

class VaGlPipeline : public Renderer {
public:
    struct Params {
        SDL_Window* window;
        uint32_t card;
        uint32_t width;
        uint32_t height;
        uint32_t rotation;
        uint32_t align;
        bool absolute_mouse;
    };

public:
    VaGlPipeline(const Params& params);
    ~VaGlPipeline() override;
    bool init();
    bool bindTextures(const std::vector<void*>& textures) override;
    RenderResult render(int64_t frame) override;
    void switchStretchMode(bool stretch) override;
    void resetRenderTarget() override;
    bool present() override;
    bool waitForPipeline(int64_t max_wait_ms) override;
    void* hwDevice() override;
    void* hwContext() override;
    uint32_t displayWidth() override;
    uint32_t displayHeight() override;
    bool setDecodedFormat(DecodedFormat format) override;

private:
    bool loadFuncs();
    bool initVaDrm();
    bool initEGL();
    bool initOpenGL();
    void resizeWindow(int screen_width, int screen_height);
    RenderResult renderVideo(int64_t frame);
    RenderResult renderCursor();
    RenderResult renderPresetCursor(const lt::CursorInfo& info);
    RenderResult renderDataCursor(const lt::CursorInfo& info);
    std::tuple<GLuint, GLuint> createCursorTextures(const lt::CursorInfo& info);
    void createCursorTexture(const uint8_t* data, uint32_t w, uint32_t h);

private:
    SDL_Window* sdl_window_ = nullptr;
    uint32_t video_width_;
    uint32_t video_height_;
    uint32_t align_;
    uint32_t card_;
    uint32_t window_width_;
    uint32_t window_height_;
    int drm_fd_ = -1;
    VADisplay va_display_ = nullptr;
    EGLContext egl_context_ = nullptr;
    EGLDisplay egl_display_ = nullptr;
    EGLSurface egl_surface_ = nullptr;
    PFNEGLCREATEIMAGEKHRPROC eglCreateImageKHR_ = nullptr;
    PFNEGLDESTROYIMAGEKHRPROC eglDestroyImageKHR_ = nullptr;
    PFNGLEGLIMAGETARGETTEXTURE2DOESPROC glEGLImageTargetTexture2DOES_ = nullptr;
    PFNGLGENVERTEXARRAYSPROC glGenVertexArrays_ = nullptr;
    PFNGLBINDVERTEXARRAYPROC glBindVertexArray_ = nullptr;
    PFNGLDELETEVERTEXARRAYSPROC glDeleteVertexArrays_ = nullptr;
    GLuint shader_ = 0;
    GLuint cursor_shader_ = 0;
    GLuint textures_[2] = {0};
    GLuint cursor_textures_[2] = {0};
    GLuint vao_ = 0;
    GLuint vbo_ = 0;
    GLuint ebo_ = 0;
    GLuint cursor_vao_ = 0;
    GLuint cursor_vbo_ = 0;
    GLuint cursor_ebo_ = 0;
};

} // namespace video

} // namespace lt