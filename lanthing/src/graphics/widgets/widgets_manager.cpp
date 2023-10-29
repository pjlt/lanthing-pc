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
// #include <cmrc/cmrc.hpp>
#include <SDL.h>
#include <imgui.h>

#include "control_bar_widget.h"
#include "statistics_widget.h"
#include "status_widget.h"
#include <graphics/renderer/renderer_grab_inputs.h>

// CMRC_DECLARE(fonts);

namespace lt {

std::unique_ptr<WidgetsManager> WidgetsManager::create(const Params& params) {
    std::unique_ptr<WidgetsManager> widgets{new WidgetsManager(params)};
    return widgets;
}

WidgetsManager::WidgetsManager(const Params& params)
    : dev_{params.dev}
    , ctx_{params.ctx}
    , window_{params.window}
    , video_width_{params.video_width}
    , video_height_{params.video_height}
    , set_bitrate_{params.set_bitrate}
    , status_{std::make_shared<StatusWidget>(video_width_, video_height_)}
    , statistics_{std::make_shared<StatisticsWidget>(video_width_, video_height_)} {

    ControlBarWidget::Params control_params{};
    control_params.video_width = video_width_;
    control_params.video_height = video_height_;
    control_params.exit = []() {
        SDL_Event ev{};
        ev.type = SDL_QUIT;
        SDL_PushEvent(&ev);
    };
    control_params.toggle_fullscreen = []() {
        SDL_Event ev{};
        ev.type = SDL_USEREVENT;
        // TODO: 不要用魔数
        ev.user.code = 2;
        SDL_PushEvent(&ev);
    };
    control_params.set_bitrate = set_bitrate_;
    control_params.show_stat = [this](bool show) { show_statistics_ = show; };
    control_bar_ = std::make_shared<ControlBarWidget>(control_params);

    auto d3d11_dev = reinterpret_cast<ID3D11Device*>(params.dev);
    auto d3d11_ctx = reinterpret_cast<ID3D11DeviceContext*>(params.ctx);
    d3d11_dev->AddRef();
    d3d11_ctx->AddRef();
    initImgui();
}

void WidgetsManager::initImgui() {
    HWND hwnd = reinterpret_cast<HWND>(window_);
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(reinterpret_cast<ID3D11Device*>(dev_),
                        reinterpret_cast<ID3D11DeviceContext*>(ctx_));

    //  中文字体太大了，暂时不加上去
    //  auto& io = ImGui::GetIO();
    //  auto fs = cmrc::fonts::get_filesystem();
    //  auto font = fs.open("fonts/NotoSansSC-Medium.ttf");
    //  io.Fonts->AddFontFromMemoryTTF((void*)font.begin(), static_cast<int>(font.size()), 14,
    //  nullptr,
    //                                 io.Fonts->GetGlyphRangesChineseFull());
    status_->resize();
    setImGuiValid(); // 最后
}

void WidgetsManager::uninitImgui() {
    setImGuiInvalid(); // 最先
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
}

WidgetsManager::~WidgetsManager() {
    uninitImgui();
    auto d3d11_dev = reinterpret_cast<ID3D11Device*>(dev_);
    auto d3d11_ctx = reinterpret_cast<ID3D11DeviceContext*>(ctx_);
    d3d11_dev->Release();
    d3d11_ctx->Release();
}

void WidgetsManager::render() {
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    auto& io = ImGui::GetIO();
    if (io.DeltaTime <= 0) {
        io.DeltaTime = 0.0000001f;
    }
    ImGui::NewFrame();
    control_bar_->render();
    if (show_status_) {
        status_->render();
    }
    if (show_statistics_) {
        statistics_->render();
    }
    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

void WidgetsManager::reset() {
    uninitImgui();
    initImgui();
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

void WidgetsManager::setTaskBarPos(uint32_t direction, uint32_t left, uint32_t right, uint32_t top,
                                   uint32_t bottom) {

    status_->setTaskBarPos(direction, left, right, top, bottom);
}

void WidgetsManager::updateStatus(uint32_t rtt_ms, uint32_t fps, float loss) {
    status_->update(rtt_ms, fps, loss);
}

void WidgetsManager::updateStatistics(const VideoStatistics::Stat& statistics) {
    statistics_->update(statistics);
}

} // namespace lt