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
    : Renderer{params.absolute_mouse}
    , sdl_window_{params.window}
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
    if (cursor_textures_[0] != 0) {
        glDeleteTextures(2, cursor_textures_);
    }
    if (cursor_vao_ != 0) {
        glDeleteVertexArrays(1, &cursor_vao_);
    }
    if (cursor_vbo_ != 0) {
        glDeleteBuffers(1, &cursor_vbo_);
    }
    if (cursor_ebo_ != 0) {
        glDeleteBuffers(1, &cursor_ebo_);
    }
    if (cursor_shader_ != 0) {
        glDeleteProgram(cursor_shader_);
    }
    if (sdl_gl_context_ != nullptr) {
        SDL_GL_DeleteContext(sdl_gl_context_);
    }
}

bool VtbGlPipeline::init() {
    SDL_Window* sdl_window = reinterpret_cast<SDL_Window*>(sdl_window_);
    SDL_SysWMinfo info{};
    SDL_VERSION(&info.version);
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
    glEnable(GL_BLEND);
    RenderResult video_result = renderVideo(frame);
    if (video_result == RenderResult::Failed) {
        return video_result;
    }
    RenderResult cursor_result = renderCursor();
    if (cursor_result == RenderResult::Failed) {
        return cursor_result;
    }

    return RenderResult::Success2;
}

Renderer::RenderResult VtbGlPipeline::renderVideo(int64_t frame) {
    glUseProgram(shader_);
    glBlendFunc(GL_ONE, GL_ZERO);
    ltMapOpenGLTexture(sdl_gl_context_, textures_, frame);
    glClear(GL_COLOR_BUFFER_BIT);
    while (glGetError()) {
    }
    glBindVertexArray(vao_);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    GLenum err = glGetError();
    glBindVertexArray(0);
    for (uint32_t i = 0; i < 2U; ++i) {
        glActiveTexture(GL_TEXTURE0 + i);
        glBindTexture(GL_TEXTURE_RECTANGLE, 0);
    }
    if (err) {
        return Renderer::RenderResult::Failed;
    }
    else {
        return Renderer::RenderResult::Success2;
    }
}

Renderer::RenderResult VtbGlPipeline::renderCursor() {
    if (absolute_mouse_ || !cursor_info_.has_value()) {
        return Renderer::RenderResult::Success2;
    }
    CursorInfo& c = cursor_info_.value();
    auto [cursor1, cursor2] = createCursorTextures(c);
    if (cursor1 == 0 && cursor2 == 0) {
        // return renderPresetCursor(c);
        return Renderer::RenderResult::Success2;
    }
    else {
        return renderDataCursor(c, cursor1, cursor2);
    }
}

Renderer::RenderResult VtbGlPipeline::renderDataCursor(const lt::CursorInfo& c, GLuint cursor1,
                                                       GLuint cursor2) {
    const float x = 1.0f * c.x / c.screen_w;
    const float y = 1.0f * c.y / c.screen_h;
    const float width = 1.0f * c.w / window_width_;
    const float height = 1.0f * c.h / window_height_;
    float verts[] = {(x - .5f) * 2.f, (.5f - y) * 2.f, 0.0f, 0.0f,
                     (x - .5f + width) * 2.f, (.5f - y) * 2.f, 1.0f, 0.0f,
                     (x - .5f + width) * 2.f, (.5f - y - height) * 2.f, 1.0f, 1.0f,
                     (x - .5f) * 2.f, (.5f - y - height) * 2.f, 0.0f, 1.0f};
    glUseProgram(cursor_shader_);
    glBindVertexArray(cursor_vao_);
    glBindBuffer(GL_ARRAY_BUFFER, cursor_vbo_);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(verts), verts);
    glActiveTexture(GL_TEXTURE0);

    if (cursor1 != 0) {
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glBindTexture(GL_TEXTURE_2D, cursor1);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
        GLenum err = glGetError();
        if (err != 0) {
            LOG(ERR) << "glDrawElements(cursor1) ret " << (int)err;
            return RenderResult::Failed;
        }
    }
    if (cursor2 != 0) {
        glBlendFunc(GL_ONE_MINUS_DST_COLOR, GL_ONE_MINUS_SRC_COLOR);
        // glBlendFuncSeparate(GL_ONE_MINUS_DST_COLOR, GL_ONE_MINUS_SRC_COLOR, GL_ZERO, GL_ZERO);
        glBindTexture(GL_TEXTURE_2D, cursor2);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
        GLenum err = glGetError();
        if (err != 0) {
            LOG(ERR) << "glDrawElements(cursor1) ret " << (int)err;
            return RenderResult::Failed;
        }
    }
    return RenderResult::Success2;
}

auto VtbGlPipeline::createCursorTextures(const lt::CursorInfo& c) -> std::tuple<GLuint, GLuint> {
    if (c.data.empty()) {
        return {0, 0};
    }
    switch (c.type) {
    case lt::CursorDataType::MonoChrome:
    {
        std::vector<uint8_t> cursor1((size_t)(c.w * c.h * 4));
        std::vector<uint8_t> cursor2((size_t)(c.w * c.h * 4));
        uint32_t* cursor1_ptr = reinterpret_cast<uint32_t*>(cursor1.data());
        uint32_t* cursor2_ptr = reinterpret_cast<uint32_t*>(cursor2.data());
        uint32_t pos = 0;
        uint8_t bitmask = 0b1000'0000;
        size_t size = c.data.size() / 2;
        for (size_t i = 0; i < size; i++) {
            for (uint8_t j = 0; j < 8; j++) {
                uint8_t and_bit = (c.data[i] & (bitmask >> j)) ? 1 : 0;
                uint8_t xor_bit = (c.data[i + size] & (bitmask >> j)) ? 1 : 0;
                uint8_t type = and_bit * 2 + xor_bit;
                switch (type) {
                case 0:
                    cursor1_ptr[pos] = 0xFF000000;
                    cursor2_ptr[pos] = 0;
                    break;
                case 1:
                    cursor1_ptr[pos] = 0xFFFFFFFF;
                    cursor2_ptr[pos] = 0;
                    break;
                case 2:
                    cursor1_ptr[pos] = 0;
                    cursor2_ptr[pos] = 0;
                    break;
                case 3:
                    cursor1_ptr[pos] = 0;
                    cursor2_ptr[pos] = 0xFFFFFFFF;
                    break;
                default:
                    break;
                }
                pos += 1;
            }
        }
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, cursor_textures_[0]);
        createCursorTexture(cursor1.data(), c.w, c.h);
        glBindTexture(GL_TEXTURE_2D, cursor_textures_[1]);
        createCursorTexture(cursor2.data(), c.w, c.h);
        return {cursor_textures_[0], cursor_textures_[1]};
    }
    case lt::CursorDataType::Color:
    {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, cursor_textures_[0]);
        createCursorTexture(c.data.data(), c.w, c.h);
        return {cursor_textures_[0], 0};
    }
    case lt::CursorDataType::MaskedColor:
    {
        std::vector<uint8_t> cursor1((size_t)(c.w * c.h * 4));
        std::vector<uint8_t> cursor2((size_t)(c.w * c.h * 4));
        for (size_t offset = 0; offset < c.data.size(); offset += 4) {
            const uint32_t* pixel = reinterpret_cast<const uint32_t*>(c.data.data() + offset);
            uint32_t* ptr1 = reinterpret_cast<uint32_t*>(cursor1.data() + offset);
            uint32_t* ptr2 = reinterpret_cast<uint32_t*>(cursor2.data() + offset);
            uint32_t mask = (*pixel) & 0xFF000000;
            if (mask == 0xFF000000) {
                *ptr1 = 0;
                *ptr2 = *pixel;
            }
            else if (mask == 0) {
                *ptr1 = *pixel | 0xFF000000;
                *ptr2 = 0;
            }
            else {
                LOGF(WARNING, "Invalid MonoChrome cursor mask %#x", mask);
                return {0, 0};
            }
        }
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, cursor_textures_[0]);
        createCursorTexture(cursor1.data(), c.w, c.h);
        glBindTexture(GL_TEXTURE_2D, cursor_textures_[1]);
        createCursorTexture(cursor2.data(), c.w, c.h);
        return {cursor_textures_[0], cursor_textures_[1]};
    }
    default:
        LOG(WARNING) << "Unknown cursor data type " << (int)c.type;
        return {0, 0};
    }
}

void VtbGlPipeline::createCursorTexture(const uint8_t* data, uint32_t w, uint32_t h) {
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
}

VtbGlPipeline::RenderResult VtbGlPipeline::renderPresetCursor(const lt::CursorInfo& c) {
    (void)c;
    return RenderResult::Success2;
}

void VtbGlPipeline::switchStretchMode(bool stretch) {
    (void)stretch;
}

void VtbGlPipeline::resetRenderTarget() {
    //SDL_GL_GetDrawableSize();
    SDL_Window* sdl_window = reinterpret_cast<SDL_Window*>(sdl_window_);
    int window_width, window_height;
    SDL_GetWindowSize(sdl_window, &window_width, &window_height);
    window_width_ = static_cast<uint32_t>(window_width);
    window_height_ = static_cast<uint32_t>(window_height);
}

bool VtbGlPipeline::present() {
    ltFlushOpenGLBuffer(sdl_gl_context_);
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
    return sdl_gl_context_;
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
    LOGF(INFO, "OpenGL vendor:   %s", glGetString(GL_VENDOR));
    LOGF(INFO, "OpenGL renderer: %s", glGetString(GL_RENDERER));
    LOGF(INFO, "OpenGL version:  %s", glGetString(GL_VERSION));

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
uniform sampler2DRect uTexY, uTexC;
const mat4 yuv2rgb = mat4(
    vec4(  1.1643835616,  1.1643835616,  1.1643835616,  0.0 ),
    vec4(  0.0, -0.2132486143,  2.1124017857,  0.0 ),
    vec4(  1.7927410714, -0.5329093286,  0.0,  0.0 ),
    vec4( -0.9729450750,  0.3014826655, -1.1334022179,  1.0 ));
out vec4 oColor;
void main() {
    oColor = yuv2rgb * vec4(texture(uTexY, vTexCoord).x,
                            texture(uTexC, vTexCoord * vec2(0.5, 0.5)).xy, 1.);
}
)";
    const char* kCursorFragmentShader = R"(
#version 330
in vec2 vTexCoord;
uniform sampler2D cTex;
out vec4 oColor;
void main() {
    oColor = texture(cTex, vTexCoord).zyxw;
    //oColor = vec4(1.0, 0.0, 0.0, 1.0);
}
)";
    shader_ = glCreateProgram();
    if (!shader_) {
        LOG(ERR) << "glCreateProgram failed: " << glGetError();
        return false;
    }
    cursor_shader_ = glCreateProgram();
    if (!cursor_shader_) {
        LOG(ERR) << "glCreateProgram failed: " << glGetError();
        return false;
    }
    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    if (!vs) {
        LOG(ERR) << "glCreateShader(GL_VERTEX_SHADER) failed: " << glGetError();
        return false;
    }
    AutoGuard vs_guard{[vs]() { glDeleteShader(vs); }};
    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    if (!fs) {
        LOG(ERR) << "glCreateShader(GL_FRAGMENT_SHADER) failed: " << glGetError();
        return false;
    }
    AutoGuard fs_guard{[fs]() { glDeleteShader(fs); }};
    GLuint cfs = glCreateShader(GL_FRAGMENT_SHADER);
    if (!cfs) {
        LOG(ERR) << "glCreateShader(GL_FRAGMENT_SHADER) failed: " << glGetError();
        return false;
    }
    AutoGuard cfs_guard{[cfs]() { glDeleteShader(cfs); }};
    glShaderSource(vs, 1, &kVertexShader, nullptr);
    glShaderSource(fs, 1, &kFragmentShader, nullptr);
    glShaderSource(cfs, 1, &kCursorFragmentShader, nullptr);
    while (glGetError()) {
    }
    std::array<char, 512> buffer{0};
    GLint status;
    glCompileShader(vs);
    glGetShaderiv(vs, GL_COMPILE_STATUS, &status);
    if (status != GL_TRUE) {
        glGetShaderInfoLog(vs, buffer.size(), nullptr, buffer.data());
        LOG(ERR) << "glCompileShader(GL_VERTEX_SHADER) failed: " << buffer.data();
        return false;
    }
    glCompileShader(fs);
    glGetShaderiv(fs, GL_COMPILE_STATUS, &status);
    if (status != GL_TRUE) {
        glGetShaderInfoLog(fs, buffer.size(), nullptr, buffer.data());
        LOG(ERR) << "glCompileShader(GL_FRAGMENT_SHADER) failed: " << buffer.data();
        return false;
    }
    glCompileShader(cfs);
    glGetShaderiv(cfs, GL_COMPILE_STATUS, &status);
    if (status != GL_TRUE) {
        glGetShaderInfoLog(cfs, buffer.size(), nullptr, buffer.data());
        LOG(ERR) << "glCompileShader(GL_FRAGMENT_SHADER) failed: " << buffer.data();
        return false;
    }
    glAttachShader(shader_, vs);
    glAttachShader(shader_, fs);
    glLinkProgram(shader_);
    glGetProgramiv(shader_, GL_LINK_STATUS, &status);
    if (status != GL_TRUE) {
        glGetShaderInfoLog(shader_, buffer.size(), nullptr, buffer.data());
        LOG(ERR) << "glLinkProgram() failed: " << buffer.data();
        return false;
    }

    glUseProgram(shader_);
    glUniform1i(glGetUniformLocation(shader_, "uTexY"), 0);
    glUniform1i(glGetUniformLocation(shader_, "uTexC"), 1);
    glGenTextures(2, textures_);
    for (int i = 0; i < 2; ++i) {
        glBindTexture(GL_TEXTURE_RECTANGLE, textures_[i]);
        glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }
    glBindTexture(GL_TEXTURE_RECTANGLE, 0);

    glAttachShader(cursor_shader_, vs);
    glAttachShader(cursor_shader_, cfs);
    glLinkProgram(cursor_shader_);
    glGetProgramiv(cursor_shader_, GL_LINK_STATUS, &status);
    if (status != GL_TRUE) {
        glGetShaderInfoLog(cursor_shader_, buffer.size(), nullptr, buffer.data());
        LOG(ERR) << "glLinkProgram() failed: " << buffer.data();
        return false;
    }
    glUseProgram(cursor_shader_);
    glUniform1i(glGetUniformLocation(cursor_shader_, "cTex"), 0);
    glGenTextures(2, cursor_textures_);
    for (int i = 0; i < 2; ++i) {
        glBindTexture(GL_TEXTURE_2D, cursor_textures_[i]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }
    glBindTexture(GL_TEXTURE_2D, 0);

    // clang-format off
    float verts[] = {-1.0f, 1.0f, 0.0f, 0.0f,
                      1.0f, 1.0f, video_width_ * 1.f, 0.0f,
                      1.0f, -1.0f, video_width_ * 1.f, video_height_ * 1.f,
                      -1.0f, -1.0f, 0.0f, video_height_ * 1.f};
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

    glGenVertexArrays(1, &cursor_vao_);
    glGenBuffers(1, &cursor_vbo_);
    glGenBuffers(1, &cursor_ebo_);
    glBindVertexArray(cursor_vao_);
    glBindBuffer(GL_ARRAY_BUFFER, cursor_vbo_);
    // 不赋值行不行?
    float verts_cursor[] = {-.1f, .1f, 0.0f, 0.0f,
                            .1f, .1f, 1.0f, 0.0f,
                            .1f, -.1, 1.0f, 1.0f,
                            -.1f, -.1f, 0.0f, 1.0f};
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts_cursor), verts_cursor, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cursor_ebo_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indexes), indexes, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
    return true;
}

} // namespace video

} // namespace lt
