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

#include "widgets_manager.h"

#include <d3d11.h>

#include <backends/imgui_impl_dx11.h>
#include <backends/imgui_impl_win32.h>
#include <imgui.h>

#include "control_bar_widget.h"
#include "statistics_widget.h"
#include "status_widget.h"

namespace lt {

std::unique_ptr<WidgetsManager> WidgetsManager::create(void* dev, void* ctx, void* window,
                                                       uint32_t video_width, uint32_t video_height,
                                                       uint32_t render_width,
                                                       uint32_t render_height) {
    std::unique_ptr<WidgetsManager> widgets{new WidgetsManager(
        dev, ctx, window, video_width, video_height, render_width, render_height)};
    return widgets;
}

WidgetsManager::WidgetsManager(void* dev, void* ctx, void* window, uint32_t video_width,
                               uint32_t video_height, uint32_t render_width, uint32_t render_height)
    : dev_{dev}
    , ctx_{ctx}
    , window_{window}
    , status_{std::make_shared<StatusWidget>(video_width, video_height, render_width,
                                             render_height)}
    , statistics_{std::make_shared<StatisticsWidget>(video_width, video_height, render_width,
                                                     render_height)}
    , control_bar_{std::make_shared<ControlBarWidget>(video_width, video_height, render_width,
                                                      render_height)} {
    auto d3d11_dev = reinterpret_cast<ID3D11Device*>(dev);
    auto d3d11_ctx = reinterpret_cast<ID3D11DeviceContext*>(ctx);
    d3d11_dev->AddRef();
    d3d11_ctx->AddRef();
    HWND hwnd = reinterpret_cast<HWND>(window);
    ImGui::CreateContext();
    ImGui::StyleColorsLight();
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(d3d11_dev, d3d11_ctx);
}

WidgetsManager::~WidgetsManager() {
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    auto d3d11_dev = reinterpret_cast<ID3D11Device*>(dev_);
    auto d3d11_ctx = reinterpret_cast<ID3D11DeviceContext*>(ctx_);
    d3d11_dev->Release();
    d3d11_ctx->Release();
}

void WidgetsManager::render() {
    if (!show_status_ && !show_control_bar_ && !show_statistics_) {
        return;
    }
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    if (show_status_) {
        status_->render();
    }
    if (show_control_bar_) {
        control_bar_->render();
    }
    if (show_statistics_) {
        statistics_->render();
    }
    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

void WidgetsManager::enableStatus() {
    show_status_ = true;
}

void WidgetsManager::disableStatus() {
    show_status_ = false;
}

void WidgetsManager::enableStatistics() {
    show_statistics_ = true;
}

void WidgetsManager::disableStatistics() {
    show_statistics_ = false;
}

void WidgetsManager::enableControlBar() {
    show_control_bar_ = true;
}

void WidgetsManager::disableControlBar() {
    show_control_bar_ = false;
}

void WidgetsManager::setTaskBarPos(uint32_t direction, uint32_t left, uint32_t right, uint32_t top,
                                   uint32_t bottom) {

    status_->setTaskBarPos(direction, left, right, top, bottom);
}

void WidgetsManager::updateStatus(uint32_t delay_ms, uint32_t fps, float loss) {
    status_->update(delay_ms, fps, loss);
}

void WidgetsManager::updateStatistics() {}

} // namespace lt