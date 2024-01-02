#include "va_gl_pipeline.h"

#include <array>
#include <string>

#include <fcntl.h>
#include <unistd.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GL/gl.h>
#include <GL/glext.h>
#include <libdrm/drm_fourcc.h>

#include <va/va_drm.h>
#include <va/va_drmcommon.h>

#include <SDL.h>
#include <SDL_syswm.h>

#include <ltlib/logging.h>

// https://learnopengl.com

// TODO: 1. 回收资源 2.整理OpenGL渲染管线 3.vsync相关问题.

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

VaGlPipeline::VaGlPipeline(const Params& params)
    : sdl_window_{params.window}
    , video_width_{params.width}
    , video_height_{params.height}
    , align_{params.align}
    , card_{params.card} {}

VaGlPipeline::~VaGlPipeline() {
    if (egl_display_) {
        eglMakeCurrent(egl_display_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (egl_context_) {
            eglDestroyContext(egl_display_, egl_context_);
        }
        if (egl_surface_) {
            eglDestroySurface(egl_display_, egl_surface_);
        }
        eglTerminate(egl_display_);
    }
    if (vao_ != 0) {
        glDeleteVertexArrays_(1, &vao_);
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
    if (va_display_) {
        vaTerminate(va_display_);
    }
    if (drm_fd_ >= 0) {
        close(drm_fd_);
    }
}

bool VaGlPipeline::init() {
    if (!loadFuncs()) {
        return false;
    }
    if (!initVaDrm()) {
        return false;
    }
    if (!initEGL()) {
        return false;
    }
    if (!initOpenGL()) {
        return false;
    }
    EGLBoolean egl_ret =
        eglMakeCurrent(egl_display_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    if (egl_ret != EGL_TRUE) {
        LOG(ERR) << "eglMakeCurrent(null) return " << egl_ret << " error: " << eglGetError();
        return false;
    }
    return true;
}

bool VaGlPipeline::bindTextures(const std::vector<void*>& textures) {
    (void)textures;
    return true;
}

VideoRenderer::RenderResult VaGlPipeline::render(int64_t frame) {
    EGLBoolean egl_ret = eglMakeCurrent(egl_display_, egl_surface_, egl_surface_, egl_context_);
    if (egl_ret != EGL_TRUE) {
        LOG(ERR) << "eglMakeCurrent return " << egl_ret << " error: " << eglGetError();
        return RenderResult::Failed;
    }
    AutoGuard ag{[this]() {
        EGLBoolean egl_ret =
            eglMakeCurrent(egl_display_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (egl_ret != EGL_TRUE) {
            LOG(ERR) << "eglMakeCurrent(null) return " << egl_ret << " error: " << eglGetError();
        }
    }};
    // frame是frame->data[3]
    VASurfaceID va_surface = static_cast<VASurfaceID>(frame);
    VADRMPRIMESurfaceDescriptor prime;
    VAStatus va_status = vaExportSurfaceHandle(
        va_display_, va_surface, VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME_2,
        VA_EXPORT_SURFACE_READ_ONLY | VA_EXPORT_SURFACE_SEPARATE_LAYERS, &prime);
    if (va_status != VA_STATUS_SUCCESS) {
        LOG(ERR) << "vaExportSurfaceHandle failed: " << va_status;
        return RenderResult::Failed;
    }
    if (prime.fourcc != VA_FOURCC_NV12) {
        LOG(ERR) << "prime.fourcc != VA_FOURCC_NV12";
        return RenderResult::Failed;
    }
    // TODO:  更好的同步方式
    va_status = vaSyncSurface(va_display_, va_surface);
    if (va_status != VA_STATUS_SUCCESS) {
        LOG(ERR) << "vaSyncSurface failed: " << va_status;
        return RenderResult::Failed;
    }

    glViewport(0, 0, static_cast<GLsizei>(window_width_), static_cast<GLsizei>(window_height_));
    EGLImage images[2] = {0};
    for (size_t i = 0; i < 2; ++i) {
        constexpr uint32_t formats[2] = {DRM_FORMAT_R8, DRM_FORMAT_GR88};
        if (prime.layers[i].drm_format != formats[i]) {
            LOG(ERR) << "prime.layers[i].drm_format: " << prime.layers[i].drm_format
                     << ", formats[i]: " << formats[i];
        }
        EGLint img_attr[] = {EGL_LINUX_DRM_FOURCC_EXT,
                             static_cast<EGLint>(formats[i]),
                             EGL_WIDTH,
                             static_cast<EGLint>(prime.width / (i + 1)), // half size
                             EGL_HEIGHT,
                             static_cast<EGLint>(prime.height / (i + 1)), // for chroma
                             EGL_DMA_BUF_PLANE0_FD_EXT,
                             static_cast<EGLint>(prime.objects[prime.layers[i].object_index[0]].fd),
                             EGL_DMA_BUF_PLANE0_OFFSET_EXT,
                             static_cast<EGLint>(prime.layers[i].offset[0]),
                             EGL_DMA_BUF_PLANE0_PITCH_EXT,
                             static_cast<EGLint>(prime.layers[i].pitch[0]),
                             EGL_NONE};
        images[i] =
            eglCreateImageKHR_(egl_display_, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, NULL, img_attr);
        if (!images[i]) {
            LOG(ERR) << "eglCreateImageKHR failed: "
                     << (i ? "chroma eglCreateImageKHR" : "luma eglCreateImageKHR");
            return RenderResult::Failed;
        }
        glActiveTexture(GL_TEXTURE0 + i);
        glBindTexture(GL_TEXTURE_2D, textures_[i]);
        while (glGetError()) {
        }
        glEGLImageTargetTexture2DOES_(GL_TEXTURE_2D, images[i]);
        if (glGetError()) {
            LOG(ERR) << "glEGLImageTargetTexture2DOES failed";
            return RenderResult::Failed;
        }
    }
    for (uint32_t i = 0; i < prime.num_objects; ++i) {
        close(prime.objects[i].fd);
    }
    glClear(GL_COLOR_BUFFER_BIT);
    while (glGetError()) {
    }
    glBindVertexArray_(vao_);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    GLenum err = glGetError();
    glBindVertexArray_(0);
    if (err) {
        LOG(ERR) << "glDrawArrays failed: " << err;
        return RenderResult::Failed;
    }
    EGLBoolean egl_success = eglSwapBuffers(egl_display_, egl_surface_);
    if (egl_success != EGL_TRUE) {
        LOG(ERR) << "eglSwapBuffers failed: " << eglGetError();
        return RenderResult::Success2;
    }
    for (uint32_t i = 0; i < 2U; ++i) {
        glActiveTexture(GL_TEXTURE0 + i);
        glBindTexture(GL_TEXTURE_2D, 0);
        eglDestroyImageKHR_(egl_display_, images[i]);
    }
    return RenderResult::Success2;
}

void VaGlPipeline::updateCursor(int32_t cursor_id, float x, float y, bool visible) {
    (void)cursor_id;
    (void)x;
    (void)y;
    (void)visible;
}

void VaGlPipeline::switchMouseMode(bool absolute) {
    (void)absolute;
}

void switchStretchMode(bool stretch) {
    (void)stretch;
}

void VaGlPipeline::resetRenderTarget() {
    SDL_Window* sdl_window = reinterpret_cast<SDL_Window*>(sdl_window_);
    int window_width, window_height;
    SDL_GetWindowSize(sdl_window, &window_width, &window_height);
    window_width_ = static_cast<uint32_t>(window_width);
    window_height_ = static_cast<uint32_t>(window_height);
}

bool VaGlPipeline::present() {
    return true;
}

bool VaGlPipeline::waitForPipeline(int64_t max_wait_ms) {
    (void)max_wait_ms;
    return true;
}

void* VaGlPipeline::hwDevice() {
    return va_display_;
}

void* VaGlPipeline::hwContext() {
    return va_display_;
}

uint32_t VaGlPipeline::displayWidth() {
    return window_width_;
}

uint32_t VaGlPipeline::displayHeight() {
    return window_height_;
}

bool VaGlPipeline::loadFuncs() {
    eglCreateImageKHR_ =
        reinterpret_cast<PFNEGLCREATEIMAGEKHRPROC>(eglGetProcAddress("eglCreateImageKHR"));
    if (eglCreateImageKHR_ == nullptr) {
        LOG(ERR) << "eglGetProcAddress(eglCreateImageKHR) failed";
        return false;
    }
    eglDestroyImageKHR_ =
        reinterpret_cast<PFNEGLDESTROYIMAGEKHRPROC>(eglGetProcAddress("eglDestroyImageKHR"));
    if (eglDestroyImageKHR_ == nullptr) {
        LOG(ERR) << "eglGetProcAddress(eglDestroyImageKHR) failed";
        return false;
    }
    glEGLImageTargetTexture2DOES_ = reinterpret_cast<PFNGLEGLIMAGETARGETTEXTURE2DOESPROC>(
        eglGetProcAddress("glEGLImageTargetTexture2DOES"));
    if (glEGLImageTargetTexture2DOES_ == nullptr) {
        LOG(ERR) << "eglGetProcAddress(glEGLImageTargetTexture2DOES) failed";
        return false;
    }
    glGenVertexArrays_ =
        reinterpret_cast<PFNGLGENVERTEXARRAYSPROC>(eglGetProcAddress("glGenVertexArrays"));
    if (glGenVertexArrays_ == nullptr) {
        LOG(ERR) << "eglGetProcAddress(glGenVertexArrays) failed";
        return false;
    }
    auto glDeleteVertexArrays_ =
        reinterpret_cast<PFNGLDELETEVERTEXARRAYSPROC>(eglGetProcAddress("glDeleteVertexArrays"));
    if (glDeleteVertexArrays_ == nullptr) {
        LOG(ERR) << "eglGetProcAddress(glDeleteVertexArrays) failed";
        return false;
    }
    glBindVertexArray_ =
        reinterpret_cast<PFNGLBINDVERTEXARRAYPROC>(eglGetProcAddress("glBindVertexArray"));
    if (glBindVertexArray_ == nullptr) {
        LOG(ERR) << "eglGetProcAddress(glBindVertexArray) failed";
        return false;
    }
    return true;
}

bool VaGlPipeline::initVaDrm() {
    std::string drm_node = "/dev/dri/card" + std::to_string(card_);
    drm_fd_ = ::open(drm_node.c_str(), O_RDWR);
    if (drm_fd_ < 0) {
        LOGF(ERR, "Open drm node '%s' failed", drm_node.c_str());
        return false;
    }
    VADisplay va_display = vaGetDisplayDRM(drm_fd_);
    if (!va_display) {
        LOGF(ERR, "vaGetDisplayDRM '%s' failed", drm_node.c_str());
        return false;
    }
    int major, minor;
    VAStatus vastatus = vaInitialize(va_display, &major, &minor);
    if (vastatus != VA_STATUS_SUCCESS) {
        LOG(ERR) << "vaInitialize failed with " << (int)vastatus;
        return false;
    }
    va_display_ = va_display;
    return true;
}

bool VaGlPipeline::initEGL() {
    SDL_Window* sdl_window = reinterpret_cast<SDL_Window*>(sdl_window_);
    SDL_SysWMinfo info{};
    SDL_VERSION(&info.version);
    SDL_GetWindowWMInfo(sdl_window, &info);
    if (info.subsystem != SDL_SYSWM_X11) {
        LOG(ERR) << "Only support X11, but we are using " << (int)info.subsystem;
        return false;
    }
    int window_width, window_height;
    SDL_GetWindowSize(sdl_window, &window_width, &window_height);
    window_width_ = static_cast<uint32_t>(window_width);
    window_height_ = static_cast<uint32_t>(window_height);

    // SDL_SetHint(SDL_HINT_OPENGL_ES_DRIVER, "1");
    // SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    // SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    // SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    // SDL_GLContext gl_context = SDL_GL_CreateContext(sdl_window);
    // if (gl_context == nullptr) {
    //     LOG(ERR) << "SDL_GL_CreateContext failed: " << SDL_GetError();
    //     return false;
    // }
    // if (SDL_GL_MakeCurrent(sdl_window, gl_context) != 0) {
    //     LOG(ERR) << "SDL_GL_MakeCurrent failed: " << SDL_GetError();
    //     return false;
    // }

    egl_display_ = eglGetDisplay(reinterpret_cast<EGLNativeDisplayType>(info.info.x11.display));
    if (egl_display_ == EGL_NO_DISPLAY) {
        LOG(ERR) << "eglGetDisplay failed";
        return false;
    }
    if (!eglInitialize(egl_display_, NULL, NULL)) {
        LOG(ERR) << "eglInitialize failed";
        return false;
    }
    if (!eglBindAPI(EGL_OPENGL_API)) {
        LOG(ERR) << "eglBindAPI failed";
        return false;
    }
    EGLint visual_attr[] = {EGL_SURFACE_TYPE,
                            EGL_WINDOW_BIT,
                            EGL_RED_SIZE,
                            8,
                            EGL_GREEN_SIZE,
                            8,
                            EGL_BLUE_SIZE,
                            8,
                            EGL_ALPHA_SIZE,
                            8,
                            EGL_RENDERABLE_TYPE,
                            EGL_OPENGL_BIT,
                            EGL_NONE};
    EGLConfig egl_cfg{};
    EGLint egl_cfg_count{};
    EGLBoolean egl_ret = eglChooseConfig(egl_display_, visual_attr, &egl_cfg, 1, &egl_cfg_count);
    if (!egl_ret || egl_cfg_count < 1) {
        LOG(ERR) << "eglChooseConfig failed, egl_ret:" << egl_ret
                 << ", egl_cfg_count:" << egl_cfg_count;
        return false;
    }
    egl_surface_ = eglCreateWindowSurface(egl_display_, egl_cfg, info.info.x11.window, nullptr);
    if (egl_surface_ == EGL_NO_SURFACE) {
        LOG(ERR) << "eglCreateWindowSurface failed";
        return false;
    }
    constexpr EGLint CORE_PROFILE_MAJOR_VERSION = 3;
    constexpr EGLint CORE_PROFILE_MINOR_VERSION = 3;
    EGLint egl_ctx_attr[] = {EGL_CONTEXT_OPENGL_PROFILE_MASK,
                             EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT,
                             EGL_CONTEXT_MAJOR_VERSION,
                             CORE_PROFILE_MAJOR_VERSION,
                             EGL_CONTEXT_MINOR_VERSION,
                             CORE_PROFILE_MINOR_VERSION,
                             EGL_NONE};
    egl_context_ = eglCreateContext(egl_display_, egl_cfg, EGL_NO_CONTEXT, egl_ctx_attr);
    if (egl_context_ == EGL_NO_CONTEXT) {
        LOG(ERR) << "eglCreateContext failed";
        return false;
    }
    egl_ret = eglMakeCurrent(egl_display_, egl_surface_, egl_surface_, egl_context_);
    if (egl_ret != EGL_TRUE) {
        LOG(ERR) << "eglMakeCurrent failed: " << eglGetError();
        return false;
    }
    egl_ret = eglSwapInterval(egl_display_, 0);
    if (egl_ret != EGL_TRUE) {
        LOG(ERR) << "eglSwapInterval failed: " << eglGetError();
        return false;
    }
    return true;
}

bool VaGlPipeline::initOpenGL() {
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
    glGenVertexArrays_(1, &vao_);
    glGenBuffers(1, &vbo_);
    glGenBuffers(1, &ebo_);

    glBindVertexArray_(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indexes), indexes, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray_(0);
    return true;
}

} // namespace lt
