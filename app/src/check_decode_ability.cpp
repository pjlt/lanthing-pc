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

#include "check_decode_ability.h"

#if LT_WINDOWS
#include <Windows.h>
#elif LT_LINUX
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>
#endif // LT_WINDOWS, LT_LINUX

#include <ltlib/logging.h>
#include <ltlib/pragma_warning.h>
#include <ltlib/strings.h>
#include <ltlib/system.h>

#include <transport/transport.h>

namespace {
class SimpleGuard {
public:
    SimpleGuard(const std::function<void()>& cleanup)
        : cleanup_{cleanup} {}
    ~SimpleGuard() { cleanup_(); }

private:
    std::function<void()> cleanup_;
};
} // namespace

namespace lt {

#if defined(LT_WINDOWS)

WARNING_DISABLE(6335)
uint32_t checkDecodeAbility() {
    std::string program = ltlib::getProgramPath() + "\\lanthing.exe";
    std::wstring wprogram = ltlib::utf8To16(program);
    std::wstring wcmd = ltlib::utf8To16("-type worker -action check_decode");
    PROCESS_INFORMATION pi = {0};
    STARTUPINFO si = {0};
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.cb = sizeof(STARTUPINFO);
    si.wShowWindow = SW_HIDE;
    if (!CreateProcessW(const_cast<LPWSTR>(wprogram.c_str()), const_cast<LPWSTR>(wcmd.c_str()),
                        nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
        LOGF(ERR, "Check decode ability CreateProcessW failed with %#x", GetLastError());
        return 0;
    }
    SimpleGuard g1{[&pi]() {
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
    }};
    DWORD ret = WaitForSingleObject(pi.hProcess, 10000);
    if (ret != WAIT_OBJECT_0) {
        LOGF(ERR, "Check decode ability WaitForSingleObject failed with ret:%u err:%#x", ret,
             GetLastError());
        return 0;
    }
    DWORD exit_code = 0;
    if (!GetExitCodeProcess(pi.hProcess, &exit_code)) {
        LOGF(ERR, "Check decode ability GetExitCodeProcess failed with %#x", GetLastError());
        return 0;
    }
    return static_cast<uint32_t>(exit_code);
}
WARNING_ENABLE(6335)

#elif defined(LT_LINUX)

uint32_t checkDecodeAbility() {
    SimpleGuard a{[]() {}};
    return VideoCodecType::H264_420 | VideoCodecType::H265_420;
#if 0
    auto process_id = fork();
    if (process_id == -1) {
        LOG(ERR) << "Launch worker fork() failed: " << errno;
        return 0;
    }
    else if (process_id == 0) {
        // is child
        std::string path = ltlib::getProgramPath() + "/lanthing";
        std::vector<std::string> args;
        std::vector<char*> argv;
        args.push_back("-type");
        args.push_back("worker");
        args.push_back("-action");
        args.push_back("check_decode");
        for (auto& arg : args) {
            argv.push_back(arg.data());
        }
        argv.push_back(nullptr);
        if (execv(path.c_str(), reinterpret_cast<char* const*>(argv.data()))) {
            // 还是同一个log文件吗？
            LOG(ERR) << "Child process: execv return " << errno;
            exit(0);
        }
        return 0;
    }
    else {
        // is parent
        int status = 0;
        int ret = waitpid(process_id, &status, 0);
        if (ret <= 0) {
            LOG(ERR) << "waitpid return " << ret << " errno " << errno;
        }
        status = status < 0 ? 0 : status;
        return static_cast<uint32_t>(status);
    }
#endif // 0
}
#else  // LT_WINDOWS | LT_LINUX
uint32_t checkDecodeAbility() {
    return VideoCodecType::H264_420 | VideoCodecType::H265_420;
}
#endif // LT_WINDOWS | LT_LINUX

} // namespace lt
