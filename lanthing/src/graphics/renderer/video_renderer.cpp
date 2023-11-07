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

#include "video_renderer.h"

#if LT_WINDOWS
#include "d3d11_pipeline.h"
#elif LT_LINUX
#include "va_gl_pipeline.h"
#else
#endif // LT_WINDOWS, LT_LINUX

#include <SDL.h>
#include <SDL_syswm.h>

namespace lt {

std::unique_ptr<VideoRenderer> lt::VideoRenderer::create(const Params& params) {
    SDL_Window* sdl_window = reinterpret_cast<SDL_Window*>(params.window);
    SDL_SysWMinfo info{};
    SDL_VERSION(&info.version);
    SDL_GetWindowWMInfo(sdl_window, &info);
#if LT_WINDOWS
    D3D11Pipeline::Params d3d11_params{};
    d3d11_params.window = info.info.win.window;
    d3d11_params.luid = params.device;
    d3d11_params.widht = params.video_width;
    d3d11_params.height = params.video_height;
    d3d11_params.align = params.align;
    auto renderer = std::make_unique<D3D11Pipeline>(d3d11_params);
    if (!renderer->init()) {
        return nullptr;
    }
    return renderer;
#elif LT_LINUX
    VaGlPipeline::Params va_gl_params{};
    va_gl_params.card = static_cast<uint32_t>(params.device);
    va_gl_params.window = sdl_window;
    va_gl_params.width = params.video_width;
    va_gl_params.height = params.video_height;
    va_gl_params.align = params.align;
    auto renderer = std::make_unique<VaGlPipeline>(va_gl_params);
    if (!renderer->init()) {
        return nullptr;
    }
    return renderer;
#else
    (void)params;
    return nullptr;
#endif //
}

} // namespace lt
