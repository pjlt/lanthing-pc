#include "pc_sdl.h"

#include <g3log/g3log.hpp>
#define SDL_MAIN_HANDLED
#include <SDL.h>

#include "pc_sdl_input.h"
#include <ltlib/threads.h>

namespace {

constexpr int32_t kUserEventResetDRPipeline = 1;

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

    void set_input_handler(const OnInputEvent& on_event) override;

private:
    void loop(std::promise<bool>& promise, const std::function<void()>& i_am_alive);
    bool init_sdl_subsystems();
    void quit_sdl_subsystems();

private: // 事件处理
    enum class DispatchResult {
        kContinue,
        kStop,
    };
    DispatchResult dispatch_sdl_event(const SDL_Event& ev);
    DispatchResult handle_sdl_user_event(const SDL_Event& ev);
    DispatchResult handle_sdl_window_event(const SDL_Event& ev);
    DispatchResult reset_dr_pipeline();
    DispatchResult handle_sdl_key_up_down(const SDL_Event& ev);
    DispatchResult handle_sdl_mouse_button_event(const SDL_Event& ev);
    DispatchResult handle_sdl_mouse_motion(const SDL_Event& ev);
    DispatchResult handle_sdl_mouse_wheel(const SDL_Event& ev);
    DispatchResult handle_sdl_controller_axis_motion(const SDL_Event& ev);
    DispatchResult handle_sdl_controller_button_event(const SDL_Event& ev);
    DispatchResult handle_sdl_controller_added(const SDL_Event& ev);
    DispatchResult handle_sdl_controller_removed(const SDL_Event& ev);
    DispatchResult handle_sdl_joy_device_added(const SDL_Event& ev);
    DispatchResult handle_sdl_touch_event(const SDL_Event& ev);

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
        [&promise, this](const std::function<void()>& i_am_alive, void*) {
            loop(promise, i_am_alive);
        },
        nullptr);
    return future.get();
}

SDL_Window* PcSdlImpl::window() {
    return window_;
}

void PcSdlImpl::set_input_handler(const OnInputEvent& on_event) {
    input_->set_input_handler(on_event);
}

void PcSdlImpl::loop(std::promise<bool>& promise, const std::function<void()>& i_am_alive) {
    if (!init_sdl_subsystems()) {
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
                               desktop_width / 6,  // x
                               desktop_height / 6, // y
                               desktop_width * 2 / 3,  // width,
                               desktop_height *2 / 3, // height,
                               SDL_WINDOW_ALLOW_HIGHDPI);

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

        switch (dispatch_sdl_event(ev)) {
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
    quit_sdl_subsystems();
    on_exit_();
}

bool PcSdlImpl::init_sdl_subsystems() {
    int ret = SDL_InitSubSystem(SDL_INIT_VIDEO);
    if (ret != 0) {
        LOG(WARNING) << "SDL_INIT_VIDEO failed:" << SDL_GetError();
        return false;
    }
    ret = SDL_InitSubSystem(SDL_INIT_AUDIO);
    if (ret != 0) {
        LOG(WARNING) << "SDL_INIT_AUDIO failed:" << SDL_GetError();
        return false;
    }
    ret = SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER);
    if (ret != 0) {
        LOG(WARNING) << "SDL_INIT_GAMECONTROLLER failed:" << SDL_GetError();
        return false;
    }
    return true;
}

void PcSdlImpl::quit_sdl_subsystems() {
    SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER);
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

PcSdlImpl::DispatchResult PcSdlImpl::dispatch_sdl_event(const SDL_Event& ev) {
    switch (ev.type) {
    case SDL_QUIT:
        LOG(INFO) << "SDL_QUIT event received";
        return DispatchResult::kStop;
    case SDL_USEREVENT:
        return handle_sdl_user_event(ev);
    case SDL_WINDOWEVENT:
        return handle_sdl_window_event(ev);
    case SDL_RENDER_DEVICE_RESET:
    case SDL_RENDER_TARGETS_RESET:
        return reset_dr_pipeline();
    case SDL_KEYUP:
    case SDL_KEYDOWN:
        return handle_sdl_key_up_down(ev);
    case SDL_MOUSEBUTTONDOWN:
    case SDL_MOUSEBUTTONUP:
        return handle_sdl_mouse_button_event(ev);
    case SDL_MOUSEMOTION:
        return handle_sdl_mouse_motion(ev);
    case SDL_MOUSEWHEEL:
        return handle_sdl_mouse_wheel(ev);
    case SDL_CONTROLLERAXISMOTION:
        return handle_sdl_controller_axis_motion(ev);
    case SDL_CONTROLLERBUTTONDOWN:
    case SDL_CONTROLLERBUTTONUP:
        return handle_sdl_controller_button_event(ev);
    case SDL_CONTROLLERDEVICEADDED:
        return handle_sdl_controller_added(ev);
    case SDL_CONTROLLERDEVICEREMOVED:
        return handle_sdl_controller_removed(ev);
    case SDL_JOYDEVICEADDED:
        return handle_sdl_joy_device_added(ev);
    case SDL_FINGERDOWN:
    case SDL_FINGERMOTION:
    case SDL_FINGERUP:
        return handle_sdl_touch_event(ev);
    default:
        return DispatchResult::kContinue;
    }
}

PcSdlImpl::DispatchResult PcSdlImpl::handle_sdl_user_event(const SDL_Event& ev) {
    // 用户自定义事件
    switch (ev.user.code) {
    case kUserEventResetDRPipeline:
        return reset_dr_pipeline();
    default:
        SDL_assert(false);
        return DispatchResult::kStop;
    }
}

PcSdlImpl::DispatchResult PcSdlImpl::handle_sdl_window_event(const SDL_Event& ev) {
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
    return reset_dr_pipeline();
}

PcSdlImpl::DispatchResult PcSdlImpl::reset_dr_pipeline() {
    SDL_PumpEvents();
    // 清除已有的重置信号
    SDL_FlushEvent(SDL_RENDER_DEVICE_RESET);
    SDL_FlushEvent(SDL_RENDER_TARGETS_RESET);
    on_reset_();

    return DispatchResult::kContinue;
}

PcSdlImpl::DispatchResult PcSdlImpl::handle_sdl_key_up_down(const SDL_Event& ev) {
    input_->handle_key_up_down(ev.key);
    return DispatchResult::kContinue;
}

PcSdlImpl::DispatchResult PcSdlImpl::handle_sdl_mouse_button_event(const SDL_Event& ev) {
    input_->handle_mouse_button(ev.button);
    return DispatchResult::kContinue;
}

PcSdlImpl::DispatchResult PcSdlImpl::handle_sdl_mouse_motion(const SDL_Event& ev) {
    input_->handle_mouse_move(ev.motion);
    return DispatchResult::kContinue;
}

PcSdlImpl::DispatchResult PcSdlImpl::handle_sdl_mouse_wheel(const SDL_Event& ev) {
    input_->handle_mouse_wheel(ev.wheel);
    return DispatchResult::kContinue;
}

PcSdlImpl::DispatchResult PcSdlImpl::handle_sdl_controller_axis_motion(const SDL_Event& ev) {
    input_->handle_controller_axis(ev.caxis);
    return DispatchResult::kContinue;
}

PcSdlImpl::DispatchResult PcSdlImpl::handle_sdl_controller_button_event(const SDL_Event& ev) {
    input_->handle_controller_button(ev.cbutton);
    return DispatchResult::kContinue;
}

PcSdlImpl::DispatchResult PcSdlImpl::handle_sdl_controller_added(const SDL_Event& ev) {
    input_->handle_controller_added(ev.cdevice);
    return DispatchResult::kContinue;
}

PcSdlImpl::DispatchResult PcSdlImpl::handle_sdl_controller_removed(const SDL_Event& ev) {
    input_->handle_controller_removed(ev.cdevice);
    return DispatchResult::kContinue;
}

PcSdlImpl::DispatchResult PcSdlImpl::handle_sdl_joy_device_added(const SDL_Event& ev) {
    input_->handle_joystick_added(ev.jdevice);
    return DispatchResult::kContinue;
}

PcSdlImpl::DispatchResult PcSdlImpl::handle_sdl_touch_event(const SDL_Event& ev) {
    (void)ev;
    return DispatchResult::kContinue;
}

} // namespace lt