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

#include <optional>
#include <exception>
#include <stdexcept>

#define SDL_MAIN_HANDLED
#include <SDL.h>

#include <ltlib/logging.h>
#include <ltlib/threads.h>
#include <ltproto/client2worker/cursor_info.pb.h>

#include <video/renderer/renderer_grab_inputs.h>

#include "pc_sdl_input.h"

namespace {

constexpr int32_t kUserEventResetDRPipeline = 1;
constexpr int32_t kUserEventToggleFullscreen = 2;
constexpr int32_t kUserEventStop = 3;
constexpr int32_t kUserEventSetTitle = 4;
constexpr int32_t kUserEventSwitchMouseMode = 5;
constexpr int32_t kUserEventUpdateCursorInfo = 6;

//int sdl_event_watcher(void* userdata, SDL_Event* ev) {
//    if (ev->type == SDL_WINDOWEVENT) {
//        auto i_am_alive = (std::function<void()>*)userdata;
//        i_am_alive->operator()();
//    }
//    return 0;
//}

} // namespace

namespace lt {

namespace plat {

class PcSdlImpl {
public:
    PcSdlImpl(const PcSdl::Params& params);
    ~PcSdlImpl();

    bool init();

    int32_t loop();

    SDL_Window* window();

    void setInputHandler(const input::OnInputEvent& on_event);

    void toggleFullscreen();

    void setTitle(const std::string& title);

    void stop();

    void switchMouseMode(bool absolute);

    void setCursorInfo(int32_t cursor_id, bool visible);

private:
    bool initSdlSubSystems();
    void quitSdlSubSystems();
    void loadCursors();
    void destroyCursors();

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
    DispatchResult handleSetTitle();
    DispatchResult handleSwitchMouseMode();
    DispatchResult handleUpdateCursorInfo();

private:
    SDL_Window* window_ = nullptr;
    const bool hide_window_;
    std::function<void()> on_reset_;
    bool windowed_fullscreen_;
    bool absolute_mouse_;
    bool cursor_visible_ = false;
    int32_t cursor_id_ = 0;
    std::mutex mutex_;
    std::unique_ptr<SdlInput> input_;
    std::string title_ = "Lanthing";
    std::map<int32_t, SDL_Cursor*> cursors_;
    bool init_dummy_audio_ = false;
    bool init_audio_ = false;
    bool init_video_ = false;
    bool init_controller_ = false;
};

PcSdlImpl::PcSdlImpl(const PcSdl::Params& params)
    : hide_window_{params.hide_window}
    , on_reset_(params.on_reset)
    , windowed_fullscreen_{params.windowed_fullscreen}
    , absolute_mouse_{params.absolute_mouse} {}

PcSdlImpl::~PcSdlImpl() {
    if (ltlib::ThreadWatcher::mainThreadID() != std::this_thread::get_id()) {
        LOG(FATAL) << "You can't run ~PcSdlImpl in non-main thread!";
    }
    destroyCursors();
    input_ = nullptr;
    if (window_) {
        SDL_DestroyWindow(window_);
    }
    quitSdlSubSystems();
}

bool PcSdlImpl::init() {
    if (ltlib::ThreadWatcher::mainThreadID() != std::this_thread::get_id()) {
        LOG(FATAL) << "You can't initialize SDL in non-main thread!";
    }
    if (!initSdlSubSystems()) {
        return false;
    }
    loadCursors();
    int desktop_width = 1920;
    int desktop_height = 1080;
    SDL_DisplayMode dm{};
    int ret = SDL_GetDesktopDisplayMode(0, &dm);
    if (ret == 0) {
        desktop_height = dm.h;
        desktop_width = dm.w;
    }
    uint32_t window_flags = SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_RESIZABLE;
    if (hide_window_) {
        window_flags = window_flags | SDL_WINDOW_HIDDEN;
    }
#ifdef LT_MAC
    window_flags = window_flags | SDL_WINDOW_OPENGL;
#endif // LT_MAC
    window_ = SDL_CreateWindow("Lanthing",
                               desktop_width / 6,      // x
                               desktop_height / 6,     // y
                               desktop_width * 2 / 3,  // width,
                               desktop_height * 2 / 3, // height,
                               window_flags);

    if (window_ == nullptr) {
        LOG(ERR) << "SDL_CreateWindow failed: " << SDL_GetError();
        return false;
    }
    SdlInput::Params input_params;
    input_params.window = window_;
    input_ = SdlInput::create(input_params);
    if (input_ == nullptr) {
        return false;
    }
    SDL_StopTextInput();
    SDL_SetHint(SDL_HINT_TIMER_RESOLUTION, "1");
    SDL_SetHint(SDL_HINT_ALLOW_ALT_TAB_WHILE_GRABBED, "0");
    SDL_SetWindowKeyboardGrab(window_, SDL_TRUE);
    SDL_SetRelativeMouseMode(absolute_mouse_ ? SDL_FALSE : SDL_TRUE);
    return true;
}

SDL_Window* PcSdlImpl::window() {
    return window_;
}

void PcSdlImpl::setInputHandler(const input::OnInputEvent& on_event) {
    input_->setInputHandler(on_event);
}

void PcSdlImpl::setTitle(const std::string& title) {
    {
        std::lock_guard lk{mutex_};
        title_ = title;
    }
    SDL_Event ev{};
    ev.type = SDL_USEREVENT;
    ev.user.code = kUserEventSetTitle;
    SDL_PushEvent(&ev);
}

void PcSdlImpl::toggleFullscreen() {
    SDL_Event ev{};
    ev.type = SDL_USEREVENT;
    ev.user.code = kUserEventToggleFullscreen;
    SDL_PushEvent(&ev);
}

void PcSdlImpl::stop() {
    SDL_Event ev{};
    ev.type = SDL_USEREVENT;
    ev.user.code = kUserEventStop;
    SDL_PushEvent(&ev);
}

void PcSdlImpl::switchMouseMode(bool absolute) {
    {
        std::lock_guard lk{mutex_};
        absolute_mouse_ = absolute;
    }
    SDL_Event ev{};
    ev.type = SDL_USEREVENT;
    ev.user.code = kUserEventSwitchMouseMode;
    SDL_PushEvent(&ev);
}

void PcSdlImpl::setCursorInfo(int32_t cursor_id, bool visible) {
    {
        std::lock_guard lk{mutex_};
        cursor_id_ = cursor_id;
        cursor_visible_ = visible;
    }
    SDL_Event ev{};
    ev.type = SDL_USEREVENT;
    ev.user.code = kUserEventUpdateCursorInfo;
    SDL_PushEvent(&ev);
}

int32_t PcSdlImpl::loop() {
    if (ltlib::ThreadWatcher::mainThreadID() != std::this_thread::get_id()) {
        LOG(FATAL) << "You can't run SDL-Loop in non-main thread!";
    }
    SDL_Event ev;
    while (true) {
        if (!SDL_WaitEventTimeout(&ev, 1000)) {
            continue;
        }
        if (video::rendererGrabInputs(&ev)) {
            continue;
        }
        switch (dispatchSdlEvent(ev)) {
        case DispatchResult::kContinue:
            continue;
        case DispatchResult::kStop:
            return 0;
        default:
            LOG(FATAL) << "Unknown SDL dispatch result";
            return 0;
        }
    }
    return 0;
}

bool PcSdlImpl::initSdlSubSystems() {
    int ret = SDL_InitSubSystem(SDL_INIT_AUDIO);
    if (ret != 0) {
        LOG(ERR) << "SDL_INIT_AUDIO failed:" << SDL_GetError();
        throw std::runtime_error{"SDL_INIT_AUDIO failed"};
    }
    init_audio_ = true;
    int audio_devices = SDL_GetNumAudioDevices(0);
    if (audio_devices < 0) {
        LOG(WARNING) << "SDL_GetNumAudioDevices return " << audio_devices;
    }
    else if (audio_devices == 0) {
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        init_audio_ = false;
        ret = SDL_AudioInit("dummy");
        if (ret != 0) {
            LOG(ERR) << "SDL_AudioInit failed:" << SDL_GetError();
            throw std::runtime_error{"SDL_AudioInit failed"};
        }
        init_dummy_audio_ = true;
    }
    ret = SDL_InitSubSystem(SDL_INIT_VIDEO);
    if (ret != 0) {
        LOG(ERR) << "SDL_INIT_VIDEO failed:" << SDL_GetError();
        throw std::runtime_error{"SDL_INIT_VIDEO failed"};
    }
    init_video_ = true;
    ret = SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER);
    if (ret != 0) {
        LOG(ERR) << "SDL_INIT_GAMECONTROLLER failed:" << SDL_GetError();
        throw std::runtime_error{"SDL_INIT_GAMECONTROLLER failed"};
    }
    init_controller_ = true;
    return true;
}

void PcSdlImpl::quitSdlSubSystems() {
    if (init_controller_) {
        SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER);
    }
    if (init_video_) {
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
    }
    if (init_audio_) {
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
    }
    if (init_dummy_audio_) {
        SDL_AudioQuit();
    }
}

void PcSdlImpl::loadCursors() {
    // 顺序不一致，不能这么搞
    // for (int32_t i = 0; i < 12; i++) {
    //    cursors_[i] = SDL_CreateSystemCursor(static_cast<SDL_SystemCursor>(i));
    //}
    using namespace ltproto::client2worker;
    cursors_[CursorInfo_PresetCursor_Arrow] =
        SDL_CreateSystemCursor(SDL_SystemCursor::SDL_SYSTEM_CURSOR_ARROW);
    cursors_[CursorInfo_PresetCursor_Ibeam] =
        SDL_CreateSystemCursor(SDL_SystemCursor::SDL_SYSTEM_CURSOR_IBEAM);
    cursors_[CursorInfo_PresetCursor_Wait] =
        SDL_CreateSystemCursor(SDL_SystemCursor::SDL_SYSTEM_CURSOR_WAIT);
    cursors_[CursorInfo_PresetCursor_Cross] =
        SDL_CreateSystemCursor(SDL_SystemCursor::SDL_SYSTEM_CURSOR_CROSSHAIR);
    cursors_[CursorInfo_PresetCursor_SizeNwse] =
        SDL_CreateSystemCursor(SDL_SystemCursor::SDL_SYSTEM_CURSOR_SIZENWSE);
    cursors_[CursorInfo_PresetCursor_SizeNesw] =
        SDL_CreateSystemCursor(SDL_SystemCursor::SDL_SYSTEM_CURSOR_SIZENESW);
    cursors_[CursorInfo_PresetCursor_SizeWe] =
        SDL_CreateSystemCursor(SDL_SystemCursor::SDL_SYSTEM_CURSOR_SIZEWE);
    cursors_[CursorInfo_PresetCursor_SizeNs] =
        SDL_CreateSystemCursor(SDL_SystemCursor::SDL_SYSTEM_CURSOR_SIZENS);
    cursors_[8] = nullptr;
    cursors_[CursorInfo_PresetCursor_SizeAll] =
        SDL_CreateSystemCursor(SDL_SystemCursor::SDL_SYSTEM_CURSOR_SIZEALL);
    cursors_[CursorInfo_PresetCursor_No] =
        SDL_CreateSystemCursor(SDL_SystemCursor::SDL_SYSTEM_CURSOR_NO);
    cursors_[CursorInfo_PresetCursor_Hand] =
        SDL_CreateSystemCursor(SDL_SystemCursor::SDL_SYSTEM_CURSOR_HAND);
}

void PcSdlImpl::destroyCursors() {
    for (auto& cursor : cursors_) {
        if (cursor.second) {
            SDL_FreeCursor(cursor.second);
        }
    }
    cursors_.clear();
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
    case kUserEventSetTitle:
        return handleSetTitle();
    case kUserEventSwitchMouseMode:
        return handleSwitchMouseMode();
    case kUserEventUpdateCursorInfo:
        return handleUpdateCursorInfo();
    case kUserEventStop:
        LOG(INFO) << "SDL loop received user stop";
        return DispatchResult::kStop;
    default:
        assert(false);
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
    auto current_is_fullscreen =
        (flag & SDL_WINDOW_FULLSCREEN) || (flag & SDL_WINDOW_FULLSCREEN_DESKTOP);
    if (current_is_fullscreen) {
        SDL_SetWindowGrab(window_, SDL_FALSE);
    }
    else {
        SDL_SetWindowGrab(window_, SDL_TRUE);
    }
    SDL_WindowFlags fullscreen_mode =
        windowed_fullscreen_ ? SDL_WINDOW_FULLSCREEN_DESKTOP : SDL_WINDOW_FULLSCREEN;
    SDL_SetWindowFullscreen(window_, current_is_fullscreen ? 0 : fullscreen_mode);
    return DispatchResult::kContinue;
}

PcSdlImpl::DispatchResult PcSdlImpl::handleSetTitle() {
    std::string title;
    {
        std::lock_guard lk{mutex_};
        title = title_;
    }
    LOG(DEBUG) << "Set title " << title;
    SDL_SetWindowTitle(window_, title.c_str());
    return DispatchResult::kContinue;
}

PcSdlImpl::DispatchResult PcSdlImpl::handleSwitchMouseMode() {
    bool absolute = false;
    {
        std::lock_guard lk{mutex_};
        absolute = absolute_mouse_;
    }
    SDL_bool enable = absolute ? SDL_FALSE : SDL_TRUE;
    SDL_SetRelativeMouseMode(enable);
    return DispatchResult::kContinue;
}

PcSdlImpl::DispatchResult PcSdlImpl::handleUpdateCursorInfo() {
    int32_t cursor_id = 0;
    bool visible = false;
    bool absolute = true;
    {
        std::lock_guard<std::mutex> lk{mutex_};
        cursor_id = cursor_id_;
        visible = cursor_visible_;
        absolute = absolute_mouse_;
    }
    if (cursor_id >= 12 || !absolute) {
        return DispatchResult::kContinue;
    }
    auto iter = cursors_.find(cursor_id);
    if (iter == cursors_.cend() || iter->second == nullptr) {
        return DispatchResult::kContinue;
    }
    if (visible) {
        SDL_ShowCursor(SDL_ENABLE);
        SDL_SetCursor(iter->second);
    }
    else {
        SDL_ShowCursor(SDL_DISABLE);
    }
    return DispatchResult::kContinue;
}

std::unique_ptr<PcSdl> PcSdl::create(const Params& params) {
    if (params.on_reset == nullptr) {
        LOG(ERR) << "Create PcSdl failed: invalid parameters";
        return nullptr;
    }
    std::unique_ptr<PcSdl> sdl{new PcSdl};
    auto impl = std::make_shared<PcSdlImpl>(params);
    if (impl->init()) {
        sdl->impl_ = impl;
        return sdl;
    }
    else {
        return nullptr;
    }
}

SDL_Window* PcSdl::window() {
    return impl_->window();
}

int32_t PcSdl::loop() {
    return impl_->loop();
}

void PcSdl::setInputHandler(const input::OnInputEvent& handler) {
    impl_->setInputHandler(handler);
}

void PcSdl::toggleFullscreen() {
    impl_->toggleFullscreen();
}

void PcSdl::setTitle(const std::string& title) {
    impl_->setTitle(title);
}

void PcSdl::stop() {
    impl_->stop();
}

void PcSdl::switchMouseMode(bool absolute) {
    impl_->switchMouseMode(absolute);
}

void PcSdl::setCursorInfo(int32_t cursor_id, bool visible) {
    impl_->setCursorInfo(cursor_id, visible);
}

PcSdl::PcSdl() {}

} // namespace plat

} // namespace lt
