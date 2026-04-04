/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2026 Zhennan Tu <zhennan.tu@gmail.com>
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

#include <ltlib/thread_name_internal.h>

namespace ltlib {
namespace internal {

using SetThreadDescriptionFunc = HRESULT(WINAPI*)(HANDLE hThread, PCWSTR lpThreadDescription);

void set_current_thread_name(const char* name) {
    static auto set_thread_description_func = reinterpret_cast<SetThreadDescriptionFunc>(
        ::GetProcAddress(::GetModuleHandleA("Kernel32.dll"), "SetThreadDescription"));
    if (set_thread_description_func) {
        wchar_t wide_thread_name[64];
        for (size_t i = 0; i < (sizeof(wide_thread_name) / sizeof(wide_thread_name[0])) - 1; ++i) {
            wide_thread_name[i] = name[i];
            if (wide_thread_name[i] == L'\0') {
                break;
            }
        }
        wide_thread_name[sizeof(wide_thread_name) / sizeof(wide_thread_name[0]) - 1] = L'\0';
        set_thread_description_func(::GetCurrentThread(), wide_thread_name);
    }

#pragma pack(push, 8)
    struct {
        DWORD dwType;
        LPCSTR szName;
        DWORD dwThreadID;
        DWORD dwFlags;
    } threadname_info = {0x1000, name, static_cast<DWORD>(-1), 0};
#pragma pack(pop)

#pragma warning(push)
#pragma warning(disable : 6320 6322)
    __try {
        ::RaiseException(0x406D1388, 0, sizeof(threadname_info) / sizeof(ULONG_PTR),
                         reinterpret_cast<ULONG_PTR*>(&threadname_info));
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
#pragma warning(pop)
}

} // namespace internal
} // namespace ltlib
