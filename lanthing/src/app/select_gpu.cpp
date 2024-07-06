/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2024 Zhennan Tu <zhennan.tu@gmail.com>
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

#include "select_gpu.h"

#include <Windows.h>

#include <cstdint>

#include <functional>
#include <string>

#include <ltlib/logging.h>
#include <ltlib/pragma_warning.h>
#include <ltlib/strings.h>
#include <ltlib/system.h>

namespace {
class SimpleGuard {
public:
    SimpleGuard(const std::function<void()>& cleanup)
        : cleanup_{cleanup} {}
    ~SimpleGuard() {
        if (cleanup_)
            cleanup_();
    }
    void cancel() { cleanup_ = nullptr; }

private:
    std::function<void()> cleanup_;
};
} // namespace

namespace lt {

WARNING_DISABLE(6335)
void selectGPU() {
    std::string program = ltlib::getProgramPath() + "\\lanthing.exe";
    std::wstring wprogram = ltlib::utf8To16(program);
    std::wstring wcmd = ltlib::utf8To16("-type worker -action check_dupl");
    const wchar_t* kKey = L"Software\\Microsoft\\DirectX\\UserGpuPreferences";
    for (int i = 1; i <= 2; i++) {
        LOG(INFO) << "Try GpuPreference=" << i;
        std::wstring value = L"GpuPreference=";
        value += std::to_wstring(i) + L";";
        LSTATUS status =
            RegSetKeyValueW(HKEY_CURRENT_USER, kKey, wprogram.c_str(), REG_SZ, value.c_str(),
                            static_cast<DWORD>(value.size() * sizeof(std::wstring::value_type)));
        if (status != ERROR_SUCCESS) {
            LOG(ERR) << "RegSetKeyValueW(UserGpuPreferences) failed with " << status;
            continue;
        }
        SimpleGuard reg_guard{
            [wprogram, kKey]() { RegDeleteKeyValueW(HKEY_CURRENT_USER, kKey, wprogram.c_str()); }};
        PROCESS_INFORMATION pi = {0};
        STARTUPINFOW si = {0};
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.cb = sizeof(STARTUPINFO);
        si.wShowWindow = SW_HIDE;
        if (!CreateProcessW(const_cast<LPWSTR>(wprogram.c_str()), const_cast<LPWSTR>(wcmd.c_str()),
                            nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
            LOGF(ERR, "Select GPU(%d) CreateProcessW failed with %#x", i, GetLastError());
            continue;
        }
        SimpleGuard g1{[&pi]() {
            CloseHandle(pi.hThread);
            CloseHandle(pi.hProcess);
        }};
        DWORD ret = WaitForSingleObject(pi.hProcess, 1000);
        if (ret != WAIT_OBJECT_0) {
            LOGF(ERR, "Select GPU(%d) WaitForSingleObject failed with ret:%u err:%#x", i, ret,
                 GetLastError());
            continue;
        }
        DWORD exit_code = 0;
        if (!GetExitCodeProcess(pi.hProcess, &exit_code)) {
            LOGF(ERR, "Select GPU(%d) GetExitCodeProcess failed with %#x", i, GetLastError());
            continue;
        }
        if (exit_code == 0) {
            reg_guard.cancel();
            LOGF(INFO, "Select GPU(%d) success", i);
            return;
        }
        else {
            LOG(INFO) << "Select GPU(%d) failed with " << exit_code;
        }
    }
    LOG(ERR) << "Select GPU failed";
}
WARNING_ENABLE(6335)

} // namespace lt