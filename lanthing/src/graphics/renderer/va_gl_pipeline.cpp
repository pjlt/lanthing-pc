#include "va_gl_pipeline.h"

#include <array>
#include <string>

#include <fcntl.h>
#include <unistd.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GL/gl.h>
#include <GL/glext.h>
#include <va/va.h>
#include <va/va_drm.h>

#include <SDL.h>
#include <SDL_syswm.h>

#include <ltlib/logging.h>

namespace lt {

VaGlPipeline::VaGlPipeline(const Params& params)
    : sdl_window_{params.window} {}

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
    return true;
}

bool VaGlPipeline::bindTextures(const std::vector<void*>& textures) {}

VideoRenderer::RenderResult VaGlPipeline::render(int64_t frame) {}

void VaGlPipeline::updateCursor(int32_t cursor_id, float x, float y, bool visible) {}

void VaGlPipeline::switchMouseMode(bool absolute) {}

void VaGlPipeline::resetRenderTarget() {}

bool VaGlPipeline::present() {}

bool VaGlPipeline::waitForPipeline(int64_t max_wait_ms) {}

void* VaGlPipeline::hwDevice() {
    return va_display_;
}

void* VaGlPipeline::hwContext() {
    return va_display_;
}

uint32_t VaGlPipeline::displayWidth() {}

uint32_t VaGlPipeline::displayHeight() {}

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
    glBindVertexArray_ =
        reinterpret_cast<PFNGLBINDVERTEXARRAYPROC>(eglGetProcAddress("glBindVertexArray"));
    if (glBindVertexArray_ == nullptr) {
        LOG(ERR) << "eglGetProcAddress(glBindVertexArray) failed";
        return false;
    }
}

bool VaGlPipeline::initVaDrm() {
    const char* drm_node = "/dev/dri/renderD128";
    int drm_fd = ::open(drm_node, O_RDWR);
    if (drm_fd < 0) {
        LOGF(ERR, "Open drm node '%s' failed", drm_node);
        return false;
    }
    VADisplay va_display = vaGetDisplayDRM(drm_fd);
    if (!va_display) {
        LOGF(ERR, "vaGetDisplayDRM '%s' failed", drm_node);
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

    EGLDisplay egl_display =
        eglGetDisplay(reinterpret_cast<EGLNativeDisplayType>(info.info.x11.display));
    if (egl_display == EGL_NO_DISPLAY) {
        LOG(ERR) << "eglGetDisplay failed";
        return false;
    }
    if (!eglInitialize(egl_display, NULL, NULL)) {
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
    EGLBoolean egl_ret = eglChooseConfig(egl_display, visual_attr, &egl_cfg, 1, &egl_cfg_count);
    if (!egl_ret || egl_cfg_count < 1) {
        LOG(ERR) << "eglChooseConfig failed, egl_ret:" << egl_ret
                 << ", egl_cfg_count:" << egl_cfg_count;
        return false;
    }
    EGLSurface egl_surface =
        eglCreateWindowSurface(egl_display, egl_cfg, info.info.x11.window, nullptr);
    if (egl_surface == EGL_NO_SURFACE) {
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
    EGLContext egl_context = eglCreateContext(egl_display, egl_cfg, EGL_NO_CONTEXT, egl_ctx_attr);
    if (egl_context == EGL_NO_CONTEXT) {
        LOG(ERR) << "eglCreateContext failed";
        return false;
    }
    eglMakeCurrent(egl_display, egl_surface, egl_surface, egl_context);
    eglSwapInterval(egl_display, 0);
    return true;
}

bool VaGlPipeline::initOpenGL() {
    LOGF(INFO, "OpenGL vendor:   %s\n", glGetString(GL_VENDOR));
    LOGF(INFO, "OpenGL renderer: %s\n", glGetString(GL_RENDERER));
    LOGF(INFO, "OpenGL version:  %s\n", glGetString(GL_VERSION));

    GLuint vao;
    glGenVertexArrays_(1, &vao);
    glBindVertexArray_(vao);
    constexpr char* kVertexShader = R"(
#version 130
const vec2 coords[4] = vec2[]( vec2(0.,0.), vec2(1.,0.), vec2(0.,1.), vec2(1.,1.) );
uniform vec2 uTexCoordScale
out vec2 vTexCoord;
void main() {
    vec2 c = coords[gl_VertexID];
    vTexCoord = c * uTexCoordScale;
    gl_Position = vec4(c * vec2(2.,-2.) + vec2(-1.,1.), 0., 1.);
}
)";
    constexpr char* kFragmentShader = R"(
#version 130
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
    GLuint prog = glCreateProgram();
    if (!prog) {
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
        return false;
    }
    glShaderSource(vs, 1, &kVertexShader, NULL);
    glShaderSource(fs, 1, &kFragmentShader, NULL);
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
        glGetShaderInfoLog(vs, buffer.size(), nullptr, buffer.data());
        LOG(ERR) << "glCompileShader(GL_FRAGMENT_SHADER) failed: " << buffer.data();
        return false;
    }
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
}

} // namespace lt
