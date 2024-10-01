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

#include <exception>
#include <optional>
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

// int sdl_event_watcher(void* userdata, SDL_Event* ev) {
//     if (ev->type == SDL_WINDOWEVENT) {
//         auto i_am_alive = (std::function<void()>*)userdata;
//         i_am_alive->operator()();
//     }
//     return 0;
// }

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

// void printMonochromeCursor(const std::vector<uint8_t>& data, int32_t w, int32_t h) {
//     (void)w;
//     int pitch = (int)(data.size() / 2 / h);
//     std::stringstream ss;
//     for (int i = 0; i < h; i++) {
//         for (int j = 0; j < pitch; j++) {
//             for (int k = 0; k < 8; k++) {
//                 ss << (((0b1000'0000 >> k) & data[i * pitch + j]) ? 1 : 0) << ' ';
//             }
//         }
//         ss << '\n';
//     }
//     ss << '\n';
//     for (int i = 0; i < h; i++) {
//         for (int j = 0; j < pitch; j++) {
//             for (int k = 0; k < 8; k++) {
//                 ss << (((0b1000'0000 >> k) & data[pitch * h + i * pitch + j]) ? 1 : 0) << ' ';
//             }
//         }
//         ss << '\n';
//     }
//     LOG(INFO) << "Monochrome\n" << ss.str();
// }

void convertMonochromeWin32ToSDL(lt::CursorInfo& c) {
    uint8_t* ptr1 = c.data.data();
    uint8_t* ptr2 = c.data.data() + c.data.size() / 2;
    for (size_t i = 0; i < c.data.size() / 2; i++) {
        for (int shift = 0; shift < 8; shift++) {
            const uint8_t mask = 0b1000'0000 >> shift;
            uint8_t type = ((*ptr1 & mask) ? 1 : 0) * 2 + ((*ptr2 & mask) ? 1 : 0);
            switch (type) {
            case 0:
                *ptr1 |= mask;
                *ptr2 |= mask;
                break;
            case 1:
                *ptr1 &= ~mask;
                *ptr2 |= mask;
                break;
            case 2:
                *ptr1 &= ~mask;
                *ptr2 &= ~mask;
                break;
            case 3:
                *ptr1 |= mask;
                *ptr2 &= ~mask;
                break;
            default:
                break;
            }
        }
        ptr1++;
        ptr2++;
    }
}

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

    void setCursorInfo(const lt::CursorInfo& cursor_info);

    void clearCursorInfos();

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
    std::optional<lt::CursorInfo> cursor_info_;
    std::mutex mutex_;
    std::unique_ptr<SdlInput> input_;
    std::string title_ = "Lanthing";
    std::map<int32_t, SDL_Cursor*> preset_cursors_;
    std::map<int32_t, SDL_Cursor*> xcursors_;
    bool init_dummy_audio_ = false;
    bool init_audio_ = false;
    bool init_video_ = false;
    bool init_controller_ = false;
    SDL_Cursor* sdl_cursor_ = nullptr;
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
    if (sdl_cursor_) {
        SDL_FreeCursor(sdl_cursor_);
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

void PcSdlImpl::setCursorInfo(const lt::CursorInfo& cursor_info) {
    {
        std::lock_guard lk{mutex_};
        cursor_info_ = cursor_info;
    }
    SDL_Event ev{};
    ev.type = SDL_USEREVENT;
    ev.user.code = kUserEventUpdateCursorInfo;
    SDL_PushEvent(&ev);
}

void PcSdlImpl::clearCursorInfos() {
    std::lock_guard lk{mutex_};
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
    //    preset_cursors_[i] = SDL_CreateSystemCursor(static_cast<SDL_SystemCursor>(i));
    //}
    using namespace ltproto::client2worker;
    preset_cursors_[CursorInfo_PresetCursor_Arrow] =
        SDL_CreateSystemCursor(SDL_SystemCursor::SDL_SYSTEM_CURSOR_ARROW);
    preset_cursors_[CursorInfo_PresetCursor_Ibeam] =
        SDL_CreateSystemCursor(SDL_SystemCursor::SDL_SYSTEM_CURSOR_IBEAM);
    preset_cursors_[CursorInfo_PresetCursor_Wait] =
        SDL_CreateSystemCursor(SDL_SystemCursor::SDL_SYSTEM_CURSOR_WAIT);
    preset_cursors_[CursorInfo_PresetCursor_Cross] =
        SDL_CreateSystemCursor(SDL_SystemCursor::SDL_SYSTEM_CURSOR_CROSSHAIR);
    preset_cursors_[CursorInfo_PresetCursor_SizeNwse] =
        SDL_CreateSystemCursor(SDL_SystemCursor::SDL_SYSTEM_CURSOR_SIZENWSE);
    preset_cursors_[CursorInfo_PresetCursor_SizeNesw] =
        SDL_CreateSystemCursor(SDL_SystemCursor::SDL_SYSTEM_CURSOR_SIZENESW);
    preset_cursors_[CursorInfo_PresetCursor_SizeWe] =
        SDL_CreateSystemCursor(SDL_SystemCursor::SDL_SYSTEM_CURSOR_SIZEWE);
    preset_cursors_[CursorInfo_PresetCursor_SizeNs] =
        SDL_CreateSystemCursor(SDL_SystemCursor::SDL_SYSTEM_CURSOR_SIZENS);
    preset_cursors_[8] = nullptr;
    preset_cursors_[CursorInfo_PresetCursor_SizeAll] =
        SDL_CreateSystemCursor(SDL_SystemCursor::SDL_SYSTEM_CURSOR_SIZEALL);
    preset_cursors_[CursorInfo_PresetCursor_No] =
        SDL_CreateSystemCursor(SDL_SystemCursor::SDL_SYSTEM_CURSOR_NO);
    preset_cursors_[CursorInfo_PresetCursor_Hand] =
        SDL_CreateSystemCursor(SDL_SystemCursor::SDL_SYSTEM_CURSOR_HAND);
}

void PcSdlImpl::destroyCursors() {
    for (auto& cursor : preset_cursors_) {
        if (cursor.second) {
            SDL_FreeCursor(cursor.second);
        }
    }
    preset_cursors_.clear();
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
    SDL_WindowFlags fullscreen_mode =
        windowed_fullscreen_ ? SDL_WINDOW_FULLSCREEN_DESKTOP : SDL_WINDOW_FULLSCREEN;
    SDL_SetWindowFullscreen(window_, current_is_fullscreen ? 0 : fullscreen_mode);
    if (current_is_fullscreen) {
        SDL_SetWindowGrab(window_, SDL_FALSE);
    }
    else {
        SDL_SetWindowGrab(window_, SDL_TRUE);
    }
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
    bool absolute = true;
    std::optional<lt::CursorInfo> info;
    {
        std::lock_guard<std::mutex> lk{mutex_};
        info = cursor_info_;
        absolute = absolute_mouse_;
    }
    if (!absolute) {
        return DispatchResult::kContinue;
    }
    if (!info.has_value()) {
        SDL_Cursor* sdl_cursor =
            preset_cursors_[ltproto::client2worker::CursorInfo_PresetCursor_Arrow];
        if (sdl_cursor == nullptr) {
            return DispatchResult::kContinue;
        }
        SDL_ShowCursor(SDL_ENABLE);
        SDL_SetCursor(sdl_cursor);
        return DispatchResult::kContinue;
    }
    if (!info->visible) {
        SDL_ShowCursor(SDL_DISABLE);
        return DispatchResult::kContinue;
    }
    SDL_Cursor* sdl_cursor = nullptr;
    switch (info->type) {
    case CursorDataType::MaskedColor:
    {
        for (size_t offset = 0; offset < info->data.size(); offset += 4) {
            uint32_t* ptr = reinterpret_cast<uint32_t*>(info->data.data() + offset);
            uint32_t& value = *ptr;
            uint32_t mask = 0xFF000000 & value;
            if (mask == 0xFF000000) {
                value &= 0x00FFFFFF;
            }
            else if (mask == 0) {
                value |= 0xFF000000;
            }
            else {
                LOG(WARNING) << "Invalid color mask " << mask;
            }
        }
        SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormatFrom(
            info->data.data(), info->w, info->h, 32, info->pitch, SDL_PIXELFORMAT_BGRA32);
        if (surface != nullptr) {
            sdl_cursor = SDL_CreateColorCursor(surface, info->hot_x, info->hot_y);
            SDL_FreeSurface(surface);
        }
        break;
    }
    case CursorDataType::MonoChrome:
        // printMonochromeCursor(info->data, info->w, info->h / 2);
        convertMonochromeWin32ToSDL(info.value());
        sdl_cursor =
            SDL_CreateCursor(info->data.data(), info->data.data() + info->pitch * info->h / 2,
                             info->w, info->h / 2, info->hot_x, info->hot_y);
        break;
    case CursorDataType::Color:
    {
        SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormatFrom(
            info->data.data(), info->w, info->h, 32, info->pitch, SDL_PIXELFORMAT_BGRA32);
        if (surface != nullptr) {
            sdl_cursor = SDL_CreateColorCursor(surface, info->hot_x, info->hot_y);
            SDL_FreeSurface(surface);
        }
        break;
    }
    default:
        if (info->preset.has_value()) {
            if (info->preset.value() >= 12) {
                return DispatchResult::kContinue;
            }
            auto iter = preset_cursors_.find(info->preset.value());
            if (iter == preset_cursors_.cend() || iter->second == nullptr) {
                return DispatchResult::kContinue;
            }
            SDL_ShowCursor(SDL_ENABLE);
            SDL_SetCursor(iter->second);
            return DispatchResult::kContinue;
        }
        break;
    }
    if (sdl_cursor) {
        SDL_ShowCursor(SDL_ENABLE);
        SDL_SetCursor(sdl_cursor);
        if (sdl_cursor_) {
            SDL_FreeCursor(sdl_cursor_);
        }
        sdl_cursor_ = sdl_cursor;
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

void PcSdl::setCursorInfo(const lt::CursorInfo& cursor_info) {
    impl_->setCursorInfo(cursor_info);
}

void PcSdl::clearCursorInfos() {
    impl_->clearCursorInfos();
}

PcSdl::PcSdl() {}

} // namespace plat

} // namespace lt
