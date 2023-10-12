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

#include <Windows.h>

#include <sstream>

#include <ltlib/logging.h>

#include <ltlib/strings.h>
#include <ltlib/system.h>

namespace {

std::string to_string(lt::VideoCodecType codec) {
    switch (codec) {
    case lt::VideoCodecType::H264:
        return "avc";
    case lt::VideoCodecType::H265:
        return "hevc";
    default:
        return "unknown";
    }
}

} // namespace

namespace lt {

ClientSession::ClientSession(const Params& params)
    : params_(params) {}

ClientSession::~ClientSession() {
    TerminateProcess(handle_, 0);
    CloseHandle(handle_);
}

bool ClientSession::start() {
    // clang-format off
    // TODO: 改成跨平台的方式
    // TODO: 参数通过管道传输
    std::stringstream ss;
    ss << ltlib::getProgramPath<char>() << "\\"
       << "lanthing.exe "
       << " -type client"
       << " -cid " << params_.client_id
       << " -rid " << params_.room_id
       << " -token " << params_.auth_token
       << " -user " << params_.p2p_username
       << " -pwd " << params_.p2p_password
       << " -addr " << params_.signaling_addr
       << " -port " << params_.signaling_port
       << " -codec " << to_string(params_.video_codec_type)
       << " -width " << params_.width
       << " -height " << params_.height
       << " -freq " << params_.refresh_rate
       << " -dinput " << (params_.enable_driver_input ? 1 : 0)
       << " -gamepad " << (params_.enable_gamepad ? 1 : 0)
        << " -chans " << params_.audio_channels
        << " -afreq " << params_.audio_freq;
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
            stoped_ = true;
            params_.on_exited();
            return;
        default:
            break;
        }
    };
}

std::string ClientSession::clientID() const {
    return params_.client_id;
}

} // namespace lt