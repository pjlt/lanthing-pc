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

#include <ltproto/error_code.pb.h>
#include <ltproto/worker2service/stop_working.pb.h>

#include <lt_constants.h>
#include <ltlib/logging.h>
#include <ltlib/pragma_warning.h>
#include <ltlib/strings.h>
#include <ltlib/system.h>

namespace {

std::string to_string(std::vector<lt::VideoCodecType> codecs) {
    std::string str;
    for (size_t i = 0; i < codecs.size(); i++) {
        switch (codecs[i]) {
        case lt::VideoCodecType::H264_420:
            str += lt::toString(lt::VideoCodecType::H264_420);
            break;
        case lt::VideoCodecType::H265_420:
            str += lt::toString(lt::VideoCodecType::H265_420);
            break;
        default:
            str += lt::toString(lt::VideoCodecType::Unknown);
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
    : path_{params.path}
    , pipe_name_{params.pipe_name}
    , client_width_{params.client_width}
    , client_height_{params.client_height}
    , client_refresh_rate_{params.client_refresh_rate}
    , client_codecs_{params.client_codecs}
    , on_failed_{params.on_failed}
    , run_as_win_service_{ltlib::isRunAsService()} {}

WorkerProcess::~WorkerProcess() {
    stoped_ = true;
    if (process_handle_) {
        CloseHandle(process_handle_);
        process_handle_ = nullptr;
    }
    if (thread_handle_) {
        CloseHandle(thread_handle_);
        thread_handle_ = nullptr;
    }
}

void WorkerProcess::stop() {
    stoped_ = true;
}

void WorkerProcess::changeResolution(uint32_t width, uint32_t height, uint32_t monitor_index) {
    client_width_ = width;
    client_height_ = height;
    monitor_index_ = monitor_index;
}

void WorkerProcess::start() {
    std::lock_guard lk{mutex_};
    if (thread_ != nullptr) {
        LOG(WARNING) << "Host process already launched";
        return;
    }
    stoped_ = false;
    thread_ = ltlib::BlockingThread::create(
        "worker_process",
        [this](const std::function<void()>& i_am_alive) { mainLoop(i_am_alive); });
}

void WorkerProcess::mainLoop(const std::function<void()>& i_am_alive) {
    while (!stoped_) {
        i_am_alive();
        if (!launchWorkerProcess()) {
            LOG(WARNING) << "Launch worker process failed";
            std::this_thread::sleep_for(std::chrono::milliseconds{100});
            continue;
        }
        if (!waitForWorkerProcess(i_am_alive)) {
            return;
        }
    }
}

bool WorkerProcess::launchWorkerProcess() {
    std::stringstream ss;
    ss << path_ << " -type worker "
       << " -name " << pipe_name_ << " -width " << client_width_ << " -height " << client_height_
       << " -freq " << client_refresh_rate_ << " -codecs " << ::to_string(client_codecs_)
       << " -action streaming "
       << " -mindex " << monitor_index_;
    if (first_launch_) {
        first_launch_ = false;
        ss << " -negotiate 1";
    }
    else {
        ss << " -negotiate 0";
    }
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

WARNING_DISABLE(6387)
bool WorkerProcess::waitForWorkerProcess(const std::function<void()>& i_am_alive) {
    while (!stoped_) {
        i_am_alive();
        DWORD ret = WaitForSingleObject(process_handle_, 100);
        if (ret == WAIT_TIMEOUT) {
            continue;
        }
        LOG(INFO) << "Worker process exited";
        DWORD exit_code = 0;
        if (!GetExitCodeProcess(process_handle_, &exit_code)) {
            LOGF(ERR, "GetExitCodeProcess failed with %#x", GetLastError());
        }
        else {
            LOG(INFO) << "Worker exit with " << exit_code;
        }
        // 就算是非法值也不要紧
        if (process_handle_) {
            CloseHandle(process_handle_);
            process_handle_ = nullptr;
        }
        if (thread_handle_) {
            CloseHandle(thread_handle_);
            thread_handle_ = nullptr;
        }
        if (exit_code == kExitCodeOK) {
            // 正常退出
            return false;
        }
        else if (exit_code <= 255) {
            // 我们定义的错误
            ltproto::ErrorCode ec = ltproto::ErrorCode::Unknown;
            switch (exit_code) {
            case kExitCodeTimeout:
                ec = ltproto::ErrorCode::WorkerKeepAliveTimeout;
                break;
            case kExitCodeInitWorkerFailed:
                ec = ltproto::ErrorCode::ControlledInitFailed;
                break;
            case kExitCodeInitVideoFailed:
                ec = ltproto::ErrorCode::WrokerInitVideoFailed;
                break;
            case kExitCodeInitAudioFailed:
                ec = ltproto::ErrorCode::WorkerInitAudioFailed;
                break;
            case kExitCodeInitInputFailed:
                ec = ltproto::ErrorCode::WorkerInitInputFailed;
                break;
            case kExitCodeClientChangeStreamingParamsFailed:
                ec = ltproto::ErrorCode::InitDecodeRenderPipelineFailed;
                break;
            default:
                break;
            }
            // 只返回初始化错误，超时这种‘中途’错误不返回
            if (ec != ltproto::ErrorCode::WorkerKeepAliveTimeout) {
                on_failed_(ec);
            }
            return false;
        }
        else {
            LOG(INFO) << "Try restart worker";
            // Windows的错误，可能崩溃了
            // 也有可能是我自已定义的'重启'错误码
            return true;
        }
    }
    return false;
}

} // namespace svc

} // namespace lt