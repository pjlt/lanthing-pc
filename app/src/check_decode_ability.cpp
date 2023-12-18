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

namespace {
std::string toHex(const int i) {
    char buffer[50];
    snprintf(buffer, sizeof(buffer), "%x", i);

    return std::string(buffer);
}

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
    std::string program = ltlib::getProgramPath() + "\\lanthing.exe -action check_decode";
    std::wstring wprogram = ltlib::utf8To16(program);
    std::wstring wcmd = ltlib::utf8To16("-action check_decode");
    PROCESS_INFORMATION pi = {0};
    STARTUPINFO si = {0};
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.cb = sizeof(STARTUPINFO);
    si.wShowWindow = SW_HIDE;
    if (!CreateProcessW(const_cast<LPWSTR>(wprogram.c_str()), const_cast<LPWSTR>(wcmd.c_str()),
                        nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
        LOG(ERR) << "Check decode ability CreateProcessW failed with " << toHex(GetLastError());
        return 0;
    }
    SimpleGuard g1{[&pi]() {
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
    }};
    DWORD ret = WaitForSingleObject(pi.hProcess, 3000);
    if (ret != WAIT_OBJECT_0) {
        LOG(ERR) << "Check decode ability WaitForSingleObject failed with "
                 << toHex(GetLastError());
        return 0;
    }
    DWORD exit_code = 0;
    if (!GetExitCodeProcess(pi.hProcess, &exit_code)) {
        LOG(ERR) << "Check decode ability GetExitCodeProcess failed with " << toHex(GetLastError());
        return 0;
    }
    return static_cast<uint32_t>(exit_code);
}
WARNING_ENABLE(6335)

#elif defined(LT_LINUX)

uint32_t checkDecodeAbility() {
    return 0;
}
#else // LT_WINDOWS | LT_LINUX
#error Unsupported platform
#endif // LT_WINDOWS | LT_LINUX

} // namespace lt