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

#include <Windows.h>

#include <ltlib/logging.h>

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

void SessionChangeObserver::waitForChange() {
    waitingLoop();
}

void SessionChangeObserver::stop() {
    stoped_ = true;
}

void SessionChangeObserver::waitingLoop() {
    while (!stoped_) {
        if (!ltlib::isRunasLocalSystem()) {
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