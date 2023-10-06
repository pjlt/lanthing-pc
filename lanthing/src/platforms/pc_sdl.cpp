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

#include "pc_sdl.h"

#include <ltlib/logging.h>
#define SDL_MAIN_HANDLED
#include <SDL.h>

#include "pc_sdl_input.h"
#include <graphics/renderer/renderer_grab_inputs.h>
#include <ltlib/threads.h>

namespace {

constexpr int32_t kUserEventResetDRPipeline = 1;
constexpr int32_t kUserEventToggleFullscreen = 2;

int sdl_event_watcher(void* userdata, SDL_Event* ev) {
    if (ev->type == SDL_WINDOWEVENT) {
        auto i_am_alive = (std::function<void()>*)userdata;
        i_am_alive->operator()();
    }
    return 0;
}

} // namespace

namespace lt {

class PcSdlImpl : public PcSdl {
public:
    PcSdlImpl(const Params& params);
    ~PcSdlImpl() override {}
    bool init();
    SDL_Window* window() override;

    void setInputHandler(const OnInputEvent& on_event) override;

    void toggleFullscreen() override;

private:
    void loop(std::promise<bool>& promise, const std::function<void()>& i_am_alive);
    bool initSdlSubSystems();
    void quitSdlSubSystems();

private: // 事件处理
    enum class DispatchResult {
        kContinue,
        kStop,
    };
    DispatchResult dispatchSdlEvent(const SDL_Event& ev);
    DispatchResult handleSdlUserEvent(const SDL_Event& ev);
    DispatchResult handleSdlWindowEvent(const SDL_Event& ev);
    DispatchResult resetDrPipeline();
    DispatchResult handleSdlKeyUpDown(const SDL_Event& ev);
    DispatchResult handleSdlMouseButtonEvent(const SDL_Event& ev);
    DispatchResult handleSdlMouseMotion(const SDL_Event& ev);
    DispatchResult handleSdlMouseWheel(const SDL_Event& ev);
    DispatchResult handleSdlControllerAxisMotion(const SDL_Event& ev);
    DispatchResult handleSdlControllerButtonEvent(const SDL_Event& ev);
    DispatchResult handleSdlControllerAdded(const SDL_Event& ev);
    DispatchResult handleSdlControllerRemoved(const SDL_Event& ev);
    DispatchResult handleSdlJoyDeviceAdded(const SDL_Event& ev);
    DispatchResult handleSdlTouchEvent(const SDL_Event& ev);
    DispatchResult handleToggleFullscreen();

private:
    SDL_Window* window_ = nullptr;
    bool fullscreen_ = false;
    std::function<void()> on_reset_;
    std::function<void()> on_exit_;
    std::mutex mutex_;
    std::unique_ptr<SdlInput> input_;
    std::unique_ptr<ltlib::BlockingThread> thread_;
};

std::unique_ptr<PcSdl> PcSdl::create(const Params& params) {
    if (params.on_reset == nullptr || params.on_exit == nullptr) {
        return nullptr;
    }
    std::unique_ptr<PcSdlImpl> sdl{new PcSdlImpl(params)};
    if (sdl->init()) {
        return sdl;
    }
    else {
        return nullptr;
    }
}

PcSdlImpl::PcSdlImpl(const Params& params)
    : on_reset_(params.on_reset)
    , on_exit_(params.on_exit) {}

bool PcSdlImpl::init() {
    std::promise<bool> promise;
    auto future = promise.get_future();
    thread_ = ltlib::BlockingThread::create(
        "sdl_loop",
        [&promise, this](const std::function<void()>& i_am_alive) { loop(promise, i_am_alive); });
    return future.get();
}

SDL_Window* PcSdlImpl::window() {
    return window_;
}

void PcSdlImpl::setInputHandler(const OnInputEvent& on_event) {
    input_->setInputHandler(on_event);
}

void PcSdlImpl::toggleFullscreen() {
    SDL_Event ev{};
    ev.type = SDL_USEREVENT;
    ev.user.code = kUserEventToggleFullscreen;
    SDL_PushEvent(&ev);
}

void PcSdlImpl::loop(std::promise<bool>& promise, const std::function<void()>& i_am_alive) {
    if (!initSdlSubSystems()) {
        promise.set_value(false);
        return;
    }
    int desktop_width = 1920;
    int desktop_height = 1080;
    SDL_DisplayMode dm{};
    int ret = SDL_GetDesktopDisplayMode(0, &dm);
    if (ret == 0) {
        desktop_height = dm.h;
        desktop_width = dm.w;
    }
    window_ = SDL_CreateWindow("Lanthing",
                               desktop_width / 6,      // x
                               desktop_height / 6,     // y
                               desktop_width * 2 / 3,  // width,
                               desktop_height * 2 / 3, // height,
                               SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_RESIZABLE);

    if (window_ == nullptr) {
        // 出错了，退出整个client（
        promise.set_value(false);
        return;
    }
    SdlInput::Params input_params;
    input_params.window = window_;
    input_ = SdlInput::create(input_params);
    if (input_ == nullptr) {
        promise.set_value(false);
        return;
    }
    // TODO: 这里的promise.set_value()理论上应该等到Client获取到解码能力才设置值
    promise.set_value(true);
    SDL_StopTextInput();
    SDL_SetHint(SDL_HINT_TIMER_RESOLUTION, "1");

    // 在Win10下，长时间点住SDL的窗口拖动，会让SDL_WaitEventTimeout()卡住，SDL_AddEventWatch才能获取到相关事件
    // 但回调似乎是在其它线程执行，这点需要小心
    SDL_AddEventWatch(sdl_event_watcher, (void*)&i_am_alive);

    SDL_Event ev;
    while (true) {
        i_am_alive();
        if (!SDL_WaitEventTimeout(&ev, 1000)) {
            continue;
        }
        if (rendererGrabInputs(&ev)) {
            continue;
        }
        switch (dispatchSdlEvent(ev)) {
        case DispatchResult::kContinue:
            continue;
        case DispatchResult::kStop:
            goto CLEANUP;
        default:
            goto CLEANUP;
        }
    }
CLEANUP:
    // 回收资源，删除解码器等
    SDL_DestroyWindow(window_);
    quitSdlSubSystems();
    on_exit_();
}

bool PcSdlImpl::initSdlSubSystems() {
    int ret = SDL_InitSubSystem(SDL_INIT_VIDEO);
    if (ret != 0) {
        LOG(ERR) << "SDL_INIT_VIDEO failed:" << SDL_GetError();
        return false;
    }
    ret = SDL_InitSubSystem(SDL_INIT_AUDIO);
    if (ret != 0) {
        LOG(ERR) << "SDL_INIT_AUDIO failed:" << SDL_GetError();
        return false;
    }
    ret = SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER);
    if (ret != 0) {
        LOG(ERR) << "SDL_INIT_GAMECONTROLLER failed:" << SDL_GetError();
        return false;
    }
    return true;
}

void PcSdlImpl::quitSdlSubSystems() {
    SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER);
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

PcSdlImpl::DispatchResult PcSdlImpl::dispatchSdlEvent(const SDL_Event& ev) {
    switch (ev.type) {
    case SDL_QUIT:
        LOG(INFO) << "SDL_QUIT event received";
        return DispatchResult::kStop;
    case SDL_USEREVENT:
        return handleSdlUserEvent(ev);
    case SDL_WINDOWEVENT:
        return handleSdlWindowEvent(ev);
    case SDL_RENDER_DEVICE_RESET:
    case SDL_RENDER_TARGETS_RESET:
        return resetDrPipeline();
    case SDL_KEYUP:
    case SDL_KEYDOWN:
        return handleSdlKeyUpDown(ev);
    case SDL_MOUSEBUTTONDOWN:
    case SDL_MOUSEBUTTONUP:
        return handleSdlMouseButtonEvent(ev);
    case SDL_MOUSEMOTION:
        return handleSdlMouseMotion(ev);
    case SDL_MOUSEWHEEL:
        return handleSdlMouseWheel(ev);
    case SDL_CONTROLLERAXISMOTION:
        return handleSdlControllerAxisMotion(ev);
    case SDL_CONTROLLERBUTTONDOWN:
    case SDL_CONTROLLERBUTTONUP:
        return handleSdlControllerButtonEvent(ev);
    case SDL_CONTROLLERDEVICEADDED:
        return handleSdlControllerAdded(ev);
    case SDL_CONTROLLERDEVICEREMOVED:
        return handleSdlControllerRemoved(ev);
    case SDL_JOYDEVICEADDED:
        return handleSdlJoyDeviceAdded(ev);
    case SDL_FINGERDOWN:
    case SDL_FINGERMOTION:
    case SDL_FINGERUP:
        return handleSdlTouchEvent(ev);
    default:
        return DispatchResult::kContinue;
    }
}

PcSdlImpl::DispatchResult PcSdlImpl::handleSdlUserEvent(const SDL_Event& ev) {
    // 用户自定义事件
    switch (ev.user.code) {
    case kUserEventResetDRPipeline:
        return resetDrPipeline();
    case kUserEventToggleFullscreen:
        return handleToggleFullscreen();
    default:
        SDL_assert(false);
        return DispatchResult::kStop;
    }
}

PcSdlImpl::DispatchResult PcSdlImpl::handleSdlWindowEvent(const SDL_Event& ev) {
    switch (ev.window.event) {
    case SDL_WINDOWEVENT_FOCUS_LOST:
        // 窗口失焦
        return DispatchResult::kContinue;
    case SDL_WINDOWEVENT_FOCUS_GAINED:
        // 窗口获焦
        return DispatchResult::kContinue;
    case SDL_WINDOWEVENT_LEAVE:
        // 鼠标离开窗口
        return DispatchResult::kContinue;
    case SDL_WINDOWEVENT_ENTER:
        // 鼠标进入窗口
        return DispatchResult::kContinue;
    case SDL_WINDOWEVENT_CLOSE:
        // 点了右上角的“x”
        return DispatchResult::kStop;
    }

    // 如果窗口没有发生变化，则不用处理东西了
    if (ev.window.event != SDL_WINDOWEVENT_SIZE_CHANGED) {
        return DispatchResult::kContinue;
    }
    // 走到这一步，说明接下来要重置renderer和decoder
    return resetDrPipeline();
}

PcSdlImpl::DispatchResult PcSdlImpl::resetDrPipeline() {
    SDL_PumpEvents();
    // 清除已有的重置信号
    SDL_FlushEvent(SDL_RENDER_DEVICE_RESET);
    SDL_FlushEvent(SDL_RENDER_TARGETS_RESET);
    on_reset_();

    return DispatchResult::kContinue;
}

PcSdlImpl::DispatchResult PcSdlImpl::handleSdlKeyUpDown(const SDL_Event& ev) {
    input_->handleKeyUpDown(ev.key);
    return DispatchResult::kContinue;
}

PcSdlImpl::DispatchResult PcSdlImpl::handleSdlMouseButtonEvent(const SDL_Event& ev) {
    input_->handleMouseButton(ev.button);
    return DispatchResult::kContinue;
}

PcSdlImpl::DispatchResult PcSdlImpl::handleSdlMouseMotion(const SDL_Event& ev) {
    input_->handleMouseMove(ev.motion);
    return DispatchResult::kContinue;
}

PcSdlImpl::DispatchResult PcSdlImpl::handleSdlMouseWheel(const SDL_Event& ev) {
    input_->handleMouseWheel(ev.wheel);
    return DispatchResult::kContinue;
}

PcSdlImpl::DispatchResult PcSdlImpl::handleSdlControllerAxisMotion(const SDL_Event& ev) {
    input_->handleControllerAxis(ev.caxis);
    return DispatchResult::kContinue;
}

PcSdlImpl::DispatchResult PcSdlImpl::handleSdlControllerButtonEvent(const SDL_Event& ev) {
    input_->handleControllerButton(ev.cbutton);
    return DispatchResult::kContinue;
}

PcSdlImpl::DispatchResult PcSdlImpl::handleSdlControllerAdded(const SDL_Event& ev) {
    input_->handleControllerAdded(ev.cdevice);
    return DispatchResult::kContinue;
}

PcSdlImpl::DispatchResult PcSdlImpl::handleSdlControllerRemoved(const SDL_Event& ev) {
    input_->handleControllerRemoved(ev.cdevice);
    return DispatchResult::kContinue;
}

PcSdlImpl::DispatchResult PcSdlImpl::handleSdlJoyDeviceAdded(const SDL_Event& ev) {
    input_->handleJoystickAdded(ev.jdevice);
    return DispatchResult::kContinue;
}

PcSdlImpl::DispatchResult PcSdlImpl::handleSdlTouchEvent(const SDL_Event& ev) {
    (void)ev;
    return DispatchResult::kContinue;
}

PcSdlImpl::DispatchResult PcSdlImpl::handleToggleFullscreen() {
    auto flag = SDL_GetWindowFlags(window_);
    auto is_fullscreen = (flag & SDL_WINDOW_FULLSCREEN) || (flag & SDL_WINDOW_FULLSCREEN_DESKTOP);
    SDL_SetWindowFullscreen(window_, is_fullscreen ? 0 : SDL_WINDOW_FULLSCREEN);
    return DispatchResult::kContinue;
}

} // namespace lt