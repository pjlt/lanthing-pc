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

#include "worker_process.h"

#include <Windows.h>

#include <ltlib/logging.h>

#include <ltproto/peer2peer/stop_working.pb.h>

#include <ltlib/strings.h>
#include <ltlib/system.h>

namespace {

std::string to_string(std::vector<lt::VideoCodecType> codecs) {
    std::string str;
    for (size_t i = 0; i < codecs.size(); i++) {
        switch (codecs[i]) {
        case lt::VideoCodecType::H264:
            str += "avc";
            break;
        case lt::VideoCodecType::H265:
            str += "hevc";
            break;
        default:
            str += "unknown";
            break;
        }
        if (i != codecs.size() - 1) {
            str += ",";
        }
    }
    return str;
}

} // namespace

namespace lt {

namespace svc {

std::unique_ptr<WorkerProcess> WorkerProcess::create(const Params& params) {
    std::unique_ptr<WorkerProcess> process{new WorkerProcess(params)};
    process->start();
    return process;
}

WorkerProcess::WorkerProcess(const Params& params)
    : on_stoped_{params.on_stoped}
    , path_{params.path}
    , pipe_name_{params.pipe_name}
    , client_width_{params.client_width}
    , client_height_{params.client_height}
    , client_refresh_rate_{params.client_refresh_rate}
    , client_codecs_{params.client_codecs}
    , run_as_win_service_{ltlib::isRunAsService()} {}

WorkerProcess::~WorkerProcess() {}

void WorkerProcess::start() {
    std::lock_guard lk{mutex_};
    if (thread_ != nullptr) {
        LOG(WARNING) << "Host process already launched";
        return;
    }
    stoped_ = false;
    std::promise<void> promise;
    auto future = promise.get_future();
    thread_ = ltlib::BlockingThread::create(
        "worker_process", [this, &promise](const std::function<void()>& i_am_alive) {
            mainLoop(promise, i_am_alive);
        });
    future.get();
}

void WorkerProcess::mainLoop(std::promise<void>& promise, const std::function<void()>& i_am_alive) {
    while (!stoped_) {
        i_am_alive();
        if (!launchWorkerProcess()) {
            LOG(WARNING) << "Launch worker process failed";
            std::this_thread::sleep_for(std::chrono::milliseconds{100});
            continue;
        }
        promise.set_value();
        waitForWorkerProcess(i_am_alive);
    }
}

bool WorkerProcess::launchWorkerProcess() {
    std::stringstream ss;
    ss << path_ << " -type worker "
       << " -name " << pipe_name_ << " -width " << client_width_ << " -height " << client_height_
       << " -freq " << client_refresh_rate_ << " -codecs " << ::to_string(client_codecs_);
    std::wstring cmd = ltlib::utf8To16(ss.str());
    if (process_handle_) {
        CloseHandle(process_handle_);
        process_handle_ = nullptr;
    }
    if (thread_handle_) {
        CloseHandle(thread_handle_);
        thread_handle_ = nullptr;
    }
    bool ret = false;
    HANDLE proc_handle = GetCurrentProcess();
    if (!proc_handle) {
        LOG(WARNING) << "GetCurrentProcess fail: " << GetLastError();
        return ret;
    }
    HANDLE token, user_token;
    if (!OpenProcessToken(proc_handle, TOKEN_DUPLICATE, &token)) {
        LOG(WARNING) << "OpenProcessToken fail: " << GetLastError();
        return ret;
    }
    if (!DuplicateTokenEx(token, MAXIMUM_ALLOWED, 0, SecurityImpersonation, TokenPrimary,
                          &user_token)) {
        LOG(WARNING) << "DuplicateTokenEx fail: " << GetLastError();
        return ret;
    }
    if (run_as_win_service_) {
        DWORD curr_session_id = WTSGetActiveConsoleSessionId();
        if (!curr_session_id) {
            LOG(WARNING) << "WTSGetActiveConsoleSessionId fail" << GetLastError();
            return ret;
        }
        if (!SetTokenInformation(user_token, (TOKEN_INFORMATION_CLASS)TokenSessionId,
                                 &curr_session_id, sizeof(curr_session_id))) {
            LOG(WARNING) << "SetTokenInformation fail: " << GetLastError();
            return ret;
        }
        DWORD ui_access = 1;
        if (!SetTokenInformation(user_token, (TOKEN_INFORMATION_CLASS)TokenUIAccess, &ui_access,
                                 sizeof(ui_access))) {
            LOG(WARNING) << "SetTokenInformation fail: " << GetLastError();
            return ret;
        }
    }
    PROCESS_INFORMATION pi = {0};
    SECURITY_ATTRIBUTES sa = {0};
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    STARTUPINFOW si = {0};
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.cb = sizeof(STARTUPINFO);
    si.wShowWindow = SW_SHOW;
    ret = CreateProcessAsUserW(user_token, NULL, const_cast<LPWSTR>(cmd.c_str()), &sa, &sa, FALSE,
                               NORMAL_PRIORITY_CLASS, NULL, NULL, &si, &pi);
    CloseHandle(token);
    CloseHandle(user_token);
    if (!ret) {
        LOG(WARNING) << "CreateProcessAsUser fail: " << GetLastError();
        return ret;
    }
    if (pi.hProcess == nullptr || pi.hThread == nullptr) {
        LOG(WARNING) << "hProcess==nullptr or hThread==nullptr";
        return false;
    }
    process_handle_ = pi.hProcess;
    thread_handle_ = pi.hThread;
    LOGF(INFO, "Launch worker process success {%p:%lu}", process_handle_, pi.dwProcessId);
    return ret;
}

#pragma warning(disable : 6387)
void WorkerProcess::waitForWorkerProcess(const std::function<void()>& i_am_alive) {
    while (!stoped_) {
        i_am_alive();
        DWORD ret = WaitForSingleObject(process_handle_, 100);
        if (ret == WAIT_TIMEOUT) {
            continue;
        }
        CloseHandle(process_handle_);
        process_handle_ = nullptr;
        CloseHandle(thread_handle_);
        thread_handle_ = nullptr;
        stoped_ = true;
        LOG(INFO) << "Worker process exited";
    }
    on_stoped_();
}

} // namespace svc

} // namespace lt