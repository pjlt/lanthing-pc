/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2024 Zhennan Tu <zhennan.tu@gmail.com>
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

#include "vtb_gl_pipeline.h"

#include <array>
#include <string>

#include <SDL.h>
#include <SDL_syswm.h>

#include <ltlib/logging.h>
#include <video/renderer/vtb_gl_pipeline_plat.h>

#define _ALIGN(x, a) (((x) + (a)-1) & ~((a)-1))

namespace {
struct AutoGuard {
    AutoGuard(const std::function<void()>& func)
        : func_{func} {}
    ~AutoGuard() {
        if (func_) {
            func_();
        }
    }

private:
    std::function<void()> func_;
};
} // namespace

namespace lt {

namespace video {

VtbGlPipeline::VtbGlPipeline(const Params& params)
    : sdl_window_{params.window}
    , video_width_{params.width}
    , video_height_{params.height}
    , align_{params.align} {}

VtbGlPipeline::~VtbGlPipeline() {
    if (vao_ != 0) {
        glDeleteVertexArrays(1, &vao_);
    }
    if (vbo_ != 0) {
        glDeleteBuffers(1, &vbo_);
    }
    if (ebo_ != 0) {
        glDeleteBuffers(1, &ebo_);
    }

    if (shader_ != 0) {
        glDeleteProgram(shader_);
    }
    if (sdl_gl_context_ != nullptr) {
        SDL_GL_DeleteContext(sdl_gl_context_);
    }
}

bool VtbGlPipeline::init() {
    SDL_Window* sdl_window = reinterpret_cast<SDL_Window*>(sdl_window_);
    SDL_SysWMinfo info{};
    SDL_VERSION(&info.version);
    LOG(INFO) << "111";
    SDL_GetWindowWMInfo(sdl_window, &info);
    if (info.subsystem != SDL_SYSWM_COCOA) {
        LOG(ERR) << "Only support cocoa, but we are using " << (int)info.subsystem;
        return false;
    }
    int window_width, window_height;
    SDL_GetWindowSize(sdl_window, &window_width, &window_height);
    std::this_thread::sleep_for(std::chrono::milliseconds{50});

    window_width_ = static_cast<uint32_t>(window_width);
    window_height_ = static_cast<uint32_t>(window_height);

    if (!initOpenGL()) {
        return false;
    }
    return true;
}

bool VtbGlPipeline::bindTextures(const std::vector<void*>& textures) {
    (void)textures;
    return true;
}

Renderer::RenderResult VtbGlPipeline::render(int64_t frame) {
    SDL_GL_MakeCurrent(sdl_window_, sdl_gl_context_);
    (void)frame;
    ltMapOpenGLTexture(sdl_gl_context_, textures_, frame, static_cast<int32_t>(window_width_), static_cast<int32_t>(window_height_));
    glClear(GL_COLOR_BUFFER_BIT);
    while (glGetError()) {
    }
    glBindVertexArray(vao_);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    GLenum err = glGetError();
    glBindVertexArray(0);
    if (err) {
        return RenderResult::Failed;
    }
    for (uint32_t i = 0; i < 2U; ++i) {
        glActiveTexture(GL_TEXTURE0 + i);
        glBindTexture(GL_TEXTURE_2D, 0);
        //eglDestroyImageKHR_(egl_display_, images[i]);
    }
    ltFlushOpenGLBuffer(sdl_gl_context_);
    return RenderResult::Success2;
}

void VtbGlPipeline::updateCursor(int32_t cursor_id, float x, float y, bool visible) {
    (void)cursor_id;
    (void)x;
    (void)y;
    (void)visible;
}

void VtbGlPipeline::switchMouseMode(bool absolute) {
    (void)absolute;
}

void VtbGlPipeline::switchStretchMode(bool stretch) {
    (void)stretch;
}

void VtbGlPipeline::resetRenderTarget() {
    SDL_Window* sdl_window = reinterpret_cast<SDL_Window*>(sdl_window_);
    int window_width, window_height;
    SDL_GetWindowSize(sdl_window, &window_width, &window_height);
    window_width_ = static_cast<uint32_t>(window_width);
    window_height_ = static_cast<uint32_t>(window_height);
}

bool VtbGlPipeline::present() {
    return true;
}

bool VtbGlPipeline::waitForPipeline(int64_t max_wait_ms) {
    (void)max_wait_ms;
    return true;
}

void* VtbGlPipeline::hwDevice() {
    return nullptr;
}

void* VtbGlPipeline::hwContext() {
    return nullptr;
}

uint32_t VtbGlPipeline::displayWidth() {
    return window_width_;
}

uint32_t VtbGlPipeline::displayHeight() {
    return window_height_;
}

bool VtbGlPipeline::setDecodedFormat(DecodedFormat format) {
    if (format == DecodedFormat::VTB_NV12) {
        return true;
    }
    else {
        LOG(ERR) << "VtbGlPipeline doesn't support DecodedFormat " << (int)format;
        return false;
    }
}

bool VtbGlPipeline::initOpenGL() {
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    sdl_gl_context_ = SDL_GL_CreateContext(sdl_window_);
    if (sdl_gl_context_ == nullptr) {
        LOGF(ERR, "SDL_GL_CreateContext failed with %s", SDL_GetError());
        return false;
    }
    SDL_GL_MakeCurrent(sdl_window_, sdl_gl_context_);
    LOGF(INFO, "OpenGL vendor:   %s\n", glGetString(GL_VENDOR));
    LOGF(INFO, "OpenGL renderer: %s\n", glGetString(GL_RENDERER));
    LOGF(INFO, "OpenGL version:  %s\n", glGetString(GL_VERSION));

    const char* kVertexShader = R"(
#version 330
layout(location = 0) in vec2 pos;
layout(location = 1) in vec2 tex;
out vec2 vTexCoord;
void main() {
    vTexCoord = tex;
    gl_Position = vec4(pos, 0.0, 1.0);
}
)";
    const char* kFragmentShader = R"(
#version 330
in vec2 vTexCoord;
uniform sampler2D uTexY, uTexC;
const mat4 yuv2rgb = mat4(
    vec4(  1.1643835616,  1.1643835616,  1.1643835616,  0.0 ),
    vec4(  0.0, -0.2132486143,  2.1124017857,  0.0 ),
    vec4(  1.7927410714, -0.5329093286,  0.0,  0.0 ),
    vec4( -0.9729450750,  0.3014826655, -1.1334022179,  1.0 ));
out vec4 oColor;
void main() {
    oColor = yuv2rgb * vec4(texture(uTexY, vTexCoord).x,
                            texture(uTexC, vTexCoord).xy, 1.);
}
)";
    shader_ = glCreateProgram();
    if (!shader_) {
        LOG(ERR) << "glCreateProgram failed: " << glGetError();
        return false;
    }
    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    if (!vs) {
        LOG(ERR) << "glCreateShader(GL_VERTEX_SHADER) failed: " << glGetError();
        return false;
    }
    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    if (!fs) {
        LOG(ERR) << "glCreateShader(GL_FRAGMENT_SHADER) failed: " << glGetError();
        glDeleteShader(vs);
        return false;
    }
    glShaderSource(vs, 1, &kVertexShader, nullptr);
    glShaderSource(fs, 1, &kFragmentShader, nullptr);
    while (glGetError()) {
    }
    std::array<char, 512> buffer{0};
    GLint status;
    glCompileShader(vs);
    glGetShaderiv(vs, GL_COMPILE_STATUS, &status);
    if (status != GL_TRUE) {
        glGetShaderInfoLog(vs, buffer.size(), nullptr, buffer.data());
        LOG(ERR) << "glCompileShader(GL_VERTEX_SHADER) failed: " << buffer.data();
        glDeleteShader(vs);
        glDeleteShader(fs);
        return false;
    }
    glCompileShader(fs);
    glGetShaderiv(fs, GL_COMPILE_STATUS, &status);
    if (status != GL_TRUE) {
        glGetShaderInfoLog(vs, buffer.size(), nullptr, buffer.data());
        LOG(ERR) << "glCompileShader(GL_FRAGMENT_SHADER) failed: " << buffer.data();
        glDeleteShader(vs);
        glDeleteShader(fs);
        return false;
    }
    glAttachShader(shader_, vs);
    glAttachShader(shader_, fs);
    glLinkProgram(shader_);
    glGetProgramiv(shader_, GL_LINK_STATUS, &status);
    if (status != GL_TRUE) {
        glGetShaderInfoLog(vs, buffer.size(), nullptr, buffer.data());
        LOG(ERR) << "glLinkProgram() failed: " << buffer.data();
        glDeleteProgram(shader_);
        glDeleteShader(vs);
        glDeleteShader(fs);
        return false;
    }
    glDeleteShader(vs);
    glDeleteShader(fs);
    glUseProgram(shader_);
    glUniform1i(glGetUniformLocation(shader_, "uTexY"), 0);
    glUniform1i(glGetUniformLocation(shader_, "uTexC"), 1);
    glGenTextures(2, textures_);
    for (int i = 0; i < 2; ++i) {
        glBindTexture(GL_TEXTURE_2D, textures_[i]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }
    glBindTexture(GL_TEXTURE_2D, 0);

    float u = (float)video_width_ / _ALIGN(video_width_, align_);
    float v = (float)video_height_ / _ALIGN(video_height_, align_);
    // clang-format off
    float verts[] = {-1.0f, 1.0f, 0.0f, 0.0f,
                      1.0f, 1.0f, u, 0.0f,
                      1.0f, -1.0f, u, v,
                      -1.0f, -1.0f, 0.0f, v};
    static_assert(sizeof(verts) == 4*4*4);
    const uint32_t indexes[] = {0, 1, 2, 0, 2, 3};
    // clang-format on
    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    glGenBuffers(1, &ebo_);

    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indexes), indexes, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);
    return true;
}

} // namespace video

} // namespace lt
