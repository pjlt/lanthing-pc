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
#elif LT_MAC
#include "vtb_gl_pipeline.h"
#else
#endif // LT_WINDOWS, LT_LINUX

#include <SDL.h>
#include <SDL_syswm.h>

#include <ltlib/logging.h>

namespace lt {

namespace video {

std::unique_ptr<Renderer> Renderer::create(const Params& params) {
    SDL_Window* sdl_window = reinterpret_cast<SDL_Window*>(params.window);
    SDL_SysWMinfo info{};
    SDL_VERSION(&info.version);
    SDL_GetWindowWMInfo(sdl_window, &info);
#if LT_WINDOWS
    D3D11Pipeline::Params d3d11_params{};
    d3d11_params.window = info.info.win.window;
    d3d11_params.device = params.device;
    d3d11_params.context = params.context;
    d3d11_params.widht = params.video_width;
    d3d11_params.height = params.video_height;
    d3d11_params.rotation = params.rotation;
    d3d11_params.align = params.align;
    d3d11_params.stretch = params.stretch;
    d3d11_params.absolute_mouse = params.absolute_mouse;
    auto renderer = std::make_unique<D3D11Pipeline>(params, d3d11_params);
    if (!renderer->init()) {
        return nullptr;
    }
    return renderer;
#elif LT_LINUX
    VaGlPipeline::Params va_gl_params{};
    va_gl_params.card = 0; // static_cast<uint32_t>(params.device);
    va_gl_params.window = sdl_window;
    va_gl_params.width = params.video_width;
    va_gl_params.height = params.video_height;
    va_gl_params.rotation = params.rotation;
    va_gl_params.align = params.align;
    auto renderer = std::make_unique<VaGlPipeline>(va_gl_params);
    if (!renderer->init()) {
        return nullptr;
    }
    return renderer;
#elif LT_MAC
    VtbGlPipeline::Params vtb_gl_params{};
    vtb_gl_params.window = sdl_window;
    vtb_gl_params.width = params.video_width;
    vtb_gl_params.height = params.video_height;
    vtb_gl_params.rotation = params.rotation;
    vtb_gl_params.align = params.align;
    auto renderer = std::make_unique<VtbGlPipeline>(vtb_gl_params);
    if (!renderer->init()) {
        return nullptr;
    }
    return renderer;
#else
    (void)params;
    return nullptr;
#endif //
}

Renderer::Renderer(const Params& params)
    : absolute_mouse_{params.absolute_mouse}
    , color_matrix_{params.color_matrix}
    , full_range_{params.full_range} {}

Renderer::CSCMatrix Renderer::colorMatrix(ColorMatrix matrix, bool full_range) {
    // TODO: 重新校验这些矩阵.
    static const CSCMatrix kBT709Limited =
        CSCMatrix{{1.1643835616f, 0.0000000000f, 1.7927410714f, -0.9729450750f, 1.1643835616f,
                   -0.2132486143f, -0.5329093286f, 0.3014826655f, 1.1643835616f, 2.1124017857f,
                   0.00000000000f, -1.1334022179f, 0.0f, 0.0f, 0.0f, 1.0f}};
    static const CSCMatrix kBT601Limited =
        CSCMatrix{{1.1643835616f, 0.0000000000f, 1.5960267857f, -0.8707874016f, 1.1643835616f,
                   -0.3917622901f, -0.8129676476f, 0.5295939845f, 1.1643835616f, 2.0172321429f,
                   0.00000000000f, -1.0813901597f, 0.0f, 0.0f, 0.0f, 1.0f}};
    // ???
    static const CSCMatrix kBT709Full =
        CSCMatrix{{1.0000000000f, 0.0000000000f, 1.5748000000f, -0.8700000000f, 1.0000000000f,
                   -0.1873000000f, -0.4681000000f, 0.5307000000f, 1.0000000000f, 1.8556000000f,
                   0.00000000000f, -1.0813000000f, 0.0f, 0.0f, 0.0f, 1.0f}};
    static const CSCMatrix kBT601Full =
        CSCMatrix{{1.0000000000f, 0.0000000000f, 1.4020000000f, -0.7010000000f, 1.0000000000f,
                   -0.3441362865f, -0.7141362865f, 0.5291362865f, 1.0000000000f, 1.7720000000f,
                   0.00000000000f, -1.1339862865f, 0.0f, 0.0f, 0.0f, 1.0f}};
    if (matrix == ColorMatrix::BT709 && full_range) {
        return kBT709Full;
    }
    else if (matrix == ColorMatrix::BT709 && !full_range) {
        return kBT709Limited;
    }
    else if (matrix == ColorMatrix::BT601 && full_range) {
        return kBT601Full;
    }
    else if (matrix == ColorMatrix::BT601 && !full_range) {
        return kBT601Limited;
    }
    else {
        LOG(WARNING) << "Unknown color matrix " << static_cast<int>(matrix)
                     << ", use BT601 limited as default";
        return kBT601Limited;
    }
}

void Renderer::updateCursor(const std::optional<lt::CursorInfo>& cursor_info) {
    if (!cursor_info.has_value()) {
        return;
    }
    if (cursor_info->data.empty()) {
        if (!cursor_info_.has_value()) {
            cursor_info_ = lt::CursorInfo{};
        }
        cursor_info_->screen_h = cursor_info->screen_h;
        cursor_info_->screen_w = cursor_info->screen_w;
        cursor_info_->x = cursor_info->x;
        cursor_info_->y = cursor_info->y;
        cursor_info_->visible = cursor_info->visible;
    }
    else {
        cursor_info_ = cursor_info;
    }
}

void Renderer::switchMouseMode(bool absolute) {
    absolute_mouse_ = absolute;
}

bool Renderer::attachRenderContext() {
    return true;
}

bool Renderer::detachRenderContext() {
    return true;
}

} // namespace video

} // namespace lt
