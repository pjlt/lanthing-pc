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

#include "client_session.h"

#if LT_WINDOWS
#include <Windows.h>
#elif LT_LINUX
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>
#endif // LT_WINDOWS, LT_LINUX

#include <sstream>

#include <ltlib/logging.h>

#include <ltlib/strings.h>
#include <ltlib/system.h>

namespace lt {

ClientSession::ClientSession(const Params& params)
    : params_(params) {}

#if defined(LT_WINDOWS)

ClientSession::~ClientSession() {
    TerminateProcess(handle_, 0);
    CloseHandle(handle_);
}

bool ClientSession::start() {
    // clang-format off
    std::stringstream ss;
    ss << ltlib::getProgramPath() << "\\"
       << "lanthing.exe "
       << " -type client"
       << " -cid " << params_.client_id
       << " -rid " << params_.room_id
       << " -token " << params_.auth_token
       << " -user " << params_.p2p_username
       << " -pwd " << params_.p2p_password
       << " -addr " << params_.signaling_addr
       << " -port " << params_.signaling_port
       << " -codec " << toString(params_.video_codec_type)
       << " -width " << params_.width
       << " -height " << params_.height
       << " -freq " << params_.refresh_rate
       << " -dinput " << (params_.enable_driver_input ? 1 : 0)
       << " -gamepad " << (params_.enable_gamepad ? 1 : 0)
        << " -chans " << params_.audio_channels
        << " -afreq " << params_.audio_freq
        << " -rotation " << params_.rotation;
    // clang-format on
    if (!params_.reflex_servers.empty()) {
        ss << " -reflexs ";
        for (size_t i = 0; i < params_.reflex_servers.size(); i++) {
            ss << params_.reflex_servers[i];
            if (i != params_.reflex_servers.size() - 1) {
                ss << ',';
            }
            else {
                ss << ' ';
            }
        }
    }

    HANDLE token = NULL, user_token = NULL;
    HANDLE proc_handle = INVALID_HANDLE_VALUE;
    do {
        proc_handle = GetCurrentProcess();
        if (!OpenProcessToken(proc_handle, TOKEN_DUPLICATE, &token)) {
            break;
        }
        if (!DuplicateTokenEx(token, MAXIMUM_ALLOWED, 0, SecurityImpersonation, TokenPrimary,
                              &user_token)) {
            break;
        }
        PROCESS_INFORMATION pi = {0};
        SECURITY_ATTRIBUTES sa = {0};
        sa.nLength = sizeof(SECURITY_ATTRIBUTES);
        STARTUPINFO si = {0};
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.cb = sizeof(STARTUPINFO);
        si.wShowWindow = SW_SHOW;

        LOG(INFO) << "Launching client: " << ss.str();
        std::wstring cmd = ltlib::utf8To16(ss.str());
        if (!CreateProcessAsUserW(user_token, NULL, const_cast<LPWSTR>(cmd.c_str()), &sa, &sa,
                                  FALSE, NORMAL_PRIORITY_CLASS, NULL, NULL, &si, &pi)) {
            break;
        }
        CloseHandle(token);
        CloseHandle(user_token);
        CloseHandle(pi.hThread);
        process_id_ = pi.dwProcessId;
        handle_ = pi.hProcess;
        LOG(INFO) << "Client handle " << handle_;
        std::promise<void> promise;
        auto future = promise.get_future();
        thread_ = ltlib::BlockingThread::create(
            "client_session", [&promise, this](const std::function<void()>& i_am_alive) {
                promise.set_value();
                mainLoop(i_am_alive);
            });
        future.get();
        stoped_ = false;
        return true;
    } while (false);
    LOG(ERR) << "Launch client failed: " << GetLastError();
    CloseHandle(token);
    if (user_token) {
        CloseHandle(user_token);
    }
    return false;
}

void ClientSession::mainLoop(const std::function<void()>& i_am_alive) {
    // HANDLE some_handle = ::CreateEventW(NULL, FALSE, FALSE, (L"Global\\lanthing_someevent_%lu" +
    // std::to_wstring(process_id_)).c_str());
    constexpr uint32_t k500ms = 500;
    const HANDLE handles[] = {handle_};
    while (true) {
        i_am_alive();
        auto ret = WaitForMultipleObjects(sizeof(handles) / sizeof(HANDLE), handles, FALSE, k500ms);
        switch (ret - WAIT_OBJECT_0) {
        case 0:
            LOG(INFO) << "Client " << params_.client_id << " stoped";
            stoped_ = true;
            params_.on_exited();
            return;
        default:
            break;
        }
    };
}

#else // LT_WINDOWS

ClientSession::~ClientSession() {
    if (process_id_ != 0) {
        kill(static_cast<pid_t>(process_id_), SIGTERM);
    }
    if (thread_ != nullptr) {
        thread_->join();
    }
}

bool ClientSession::start() {
    process_id_ = fork();
    if (process_id_ == -1) {
        LOG(ERR) << "Launch client fork() failed: " << errno;
        return false;
    }
    else if (process_id_ == 0) {
        // is child
        std::string path = ltlib::getProgramPath() + "/lanthing";
        std::vector<std::string> args;
        std::vector<char*> argv;
        args.push_back("-type");
        args.push_back("client");
        args.push_back("-cid");
        args.push_back(params_.client_id);
        args.push_back("-rid");
        args.push_back(params_.room_id);
        args.push_back("-token");
        args.push_back(params_.auth_token);
        args.push_back("-user");
        args.push_back(params_.p2p_username);
        args.push_back("-pwd");
        args.push_back(params_.p2p_password);
        args.push_back("-addr");
        args.push_back(params_.signaling_addr);
        args.push_back("-port");
        args.push_back(std::to_string(params_.signaling_port));
        args.push_back("-codec");
        args.push_back(toString(params_.video_codec_type));
        args.push_back("-width");
        args.push_back(std::to_string(params_.width));
        args.push_back("-height");
        args.push_back(std::to_string(params_.height));
        args.push_back("-freq");
        args.push_back(std::to_string(params_.refresh_rate));
        args.push_back("-dinput");
        args.push_back("0");
        args.push_back("-gamepad");
        args.push_back("0");
        args.push_back("-chans");
        args.push_back(std::to_string(params_.audio_channels));
        args.push_back("-afreq");
        args.push_back(std::to_string(params_.audio_freq));
        for (auto& arg : args) {
            argv.push_back(arg.data());
        }
        argv.push_back(nullptr);
        if (execv(path.c_str(), reinterpret_cast<char* const*>(argv.data()))) {
            // 还是同一个log文件吗？
            LOG(ERR) << "Child process: execv return " << errno;
            exit(-1);
            return false;
        }
    }
    else {
        // is parent
        LOG(INFO) << "Client handle " << process_id_;
        std::promise<void> promise;
        auto future = promise.get_future();
        // 暂时不搞i_am_alive，后面试试pidfd_open
        // thread_ = ltlib::BlockingThread::create(
        //     "client_session", [&promise, this](const std::function<void()>& i_am_alive) {
        //         promise.set_value();
        //         mainLoop(i_am_alive);
        //     });
        thread_ = std::make_unique<std::thread>([&promise, this]() {
            promise.set_value();
            mainLoop(nullptr);
        });
        future.get();
        stoped_ = false;
        return true;
    }
    return false;
}
void ClientSession::mainLoop(const std::function<void()>& i_am_alive) {
    (void)i_am_alive;
    // constexpr uint32_t k500ms = 500;
    // 纠结要不要用pidfd_open，不用的话要绕一大圈子搞i_am_alive
    waitpid(static_cast<pid_t>(process_id_), nullptr, 0);
    params_.on_exited();
}
#endif

std::string ClientSession::clientID() const {
    return params_.client_id;
}

std::string ClientSession::roomID() const {
    return params_.room_id;
}

} // namespace lt