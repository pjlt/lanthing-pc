#include "worker_process.h"

#include <Windows.h>

#include <g3log/g3log.hpp>

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
    , run_as_win_service_{ltlib::is_run_as_service()} {}

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
        "worker_process",
        [this, &promise](const std::function<void()>& i_am_alive, void*) {
            main_loop(promise, i_am_alive);
        },
        nullptr);
    future.get();
}

void WorkerProcess::main_loop(std::promise<void>& promise,
                              const std::function<void()>& i_am_alive) {
    while (!stoped_) {
        i_am_alive();
        if (!launch_worker_process()) {
            LOG(WARNING) << "Launch worker process failed";
            std::this_thread::sleep_for(std::chrono::milliseconds{100});
            continue;
        }
        promise.set_value();
        wait_for_worker_process(i_am_alive);
    }
}

bool WorkerProcess::launch_worker_process() {
    std::stringstream ss;
    ss << path_ << " -type worker "
       << " -name " << pipe_name_ << " -width " << client_width_ << " -height " << client_height_
       << " -freq " << client_refresh_rate_ << " -codecs " << ::to_string(client_codecs_);
    std::wstring cmd = ltlib::utf8_to_utf16(ss.str());
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
    process_handle_ = pi.hProcess;
    thread_handle_ = pi.hThread;
    LOGF(INFO, "Launch worker process success {%p:%d}", process_handle_, pi.dwProcessId);
    return ret;
}

void WorkerProcess::wait_for_worker_process(const std::function<void()>& i_am_alive) {
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