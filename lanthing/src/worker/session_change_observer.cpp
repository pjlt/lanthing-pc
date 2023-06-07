#include <Windows.h>

#include <g3log/g3log.hpp>

#include <ltlib/system.h>

#include <worker/session_change_observer.h>

namespace {

bool get_desk_name(HDESK desktop, std::wstring& name) {
    DWORD name_length = 0;
    GetUserObjectInformationW(desktop, UOI_NAME, 0, 0, &name_length);
    if (!name_length) {
        LOG(WARNING) << "GetUserObjectInformationW failed: " << GetLastError();
        return false;
    }
    std::vector<TCHAR> desk_name(name_length);
    if (!GetUserObjectInformationW(desktop, UOI_NAME, desk_name.data(), name_length, 0)) {
        LOG(WARNING) << "GetUserObjectInformationW failed: " << GetLastError();
        return false;
    }
    name.assign(desk_name.begin(), desk_name.end());
    return true;
}

} // namespace

namespace lt {

namespace worker {

std::unique_ptr<SessionChangeObserver> SessionChangeObserver::create() {
    std::unique_ptr<SessionChangeObserver> observer{new SessionChangeObserver};
    DWORD current_process_id = GetCurrentProcessId();
    DWORD prev_session_id = 0;
    ProcessIdToSessionId(current_process_id, &prev_session_id);
    observer->startup_session_id_ = prev_session_id;
    DWORD current_thread_id = GetCurrentThreadId();
    HDESK desktop = GetThreadDesktop(current_thread_id);
    std::wstring prev_desk_name;
    if (!get_desk_name(desktop, prev_desk_name))
        return nullptr;
    CloseDesktop(desktop);
    observer->startup_desk_name_ = prev_desk_name;
    return observer;
}

void SessionChangeObserver::wait_for_change() {
    waiting_loop();
}

void SessionChangeObserver::stop() {
    stoped_ = true;
}

void SessionChangeObserver::waiting_loop() {
    while (!stoped_) {
        if (!ltlib::is_run_as_local_system()) {
            std::this_thread::sleep_for(std::chrono::milliseconds{100});
            continue;
        }
        DWORD curr_session_id = WTSGetActiveConsoleSessionId();
        if (curr_session_id == 0xFFFFFFFF) {
            curr_session_id = 0;
        }
        if (curr_session_id != startup_session_id_) {
            LOG(WARNING) << "SessionID changed: " << startup_session_id_ << " -> "
                         << curr_session_id;
            return;
        }
        else {
            std::this_thread::sleep_for(std::chrono::milliseconds{100});
        }
    }
}

} // namespace worker

} // namespace lt