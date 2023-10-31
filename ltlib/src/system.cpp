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

#include <Shlobj.h>
#include <TlHelp32.h>

#include <cstring>
#include <functional>
#include <string>
#include <vector>

#include <ltlib/strings.h>
#include <ltlib/system.h>

namespace {

BOOL GetTokenByName(HANDLE& hToken, const LPWSTR lpName) {
    if (!lpName)
        return FALSE;

    HANDLE hProcessSnap = NULL;
    BOOL bRet = FALSE;
    PROCESSENTRY32W pe32 = {0};

    hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hProcessSnap == INVALID_HANDLE_VALUE)
        return (FALSE);

    pe32.dwSize = sizeof(PROCESSENTRY32W);

    if (Process32FirstW(hProcessSnap, &pe32)) {
        do {
            if (!wcscmp(_wcsupr(pe32.szExeFile), _wcsupr(lpName))) {
                HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pe32.th32ProcessID);
                bRet = OpenProcessToken(hProcess, TOKEN_ALL_ACCESS, &hToken);
                CloseHandle(hProcess);
                CloseHandle(hProcessSnap);
                return (bRet);
            }
        } while (Process32NextW(hProcessSnap, &pe32));
        bRet = FALSE;
    }
    else {
        bRet = FALSE;
    }

    CloseHandle(hProcessSnap);
    return (bRet);
}

bool executeAsUser(const std::function<bool(HANDLE)>& func) {
    HANDLE hToken = NULL;
    bool res = false;
    do {
        if (!GetTokenByName(hToken, (const LPWSTR)L"EXPLORER.EXE")) {
            return false;
        }
        if (!hToken) {
            break;
        }
        if (FALSE == ImpersonateLoggedOnUser(hToken)) {
            break;
        }

        res = func(hToken);

        if (FALSE == RevertToSelf()) {
            break;
        }
    } while (0);

    if (NULL != hToken) {
        CloseHandle(hToken);
    }
    return res;
}

} // namespace

namespace ltlib {

bool getProgramFilename(std::wstring& filename) {
    const int kMaxPath = UNICODE_STRING_MAX_CHARS;
    std::vector<wchar_t> the_filename(kMaxPath);
    DWORD length = ::GetModuleFileNameW(nullptr, the_filename.data(), kMaxPath);
    if (length > 0 && length < kMaxPath) {
        filename = std::wstring(the_filename.data(), length);
        return true;
    }
    return false;
}

bool getProgramPath(std::wstring& path) {
    std::wstring filename;
    if (getProgramFilename(filename)) {
        std::wstring::size_type pos = filename.rfind(L'\\');
        if (pos != std::wstring::npos) {
            path = filename.substr(0, pos);
            return true;
        }
    }
    return false;
}

std::string getProgramPath() {
    std::wstring path;
    getProgramPath(path);
    return utf16To8(path);
}

std::string getProgramFullpath() {
    std::wstring path;
    getProgramFilename(path);
    return utf16To8(path);
}

std::string getAppdataPath(bool is_service) {
    static std::string appdata_path;
    if (!appdata_path.empty()) {
        return appdata_path;
    }
    std::wstring wappdata_path;

    auto get_path = [&](HANDLE) -> bool {
        wchar_t m_lpszDefaultDir[MAX_PATH] = {0};
        wchar_t szDocument[MAX_PATH] = {0};

        LPITEMIDLIST pidl = NULL;
        auto hr = SHGetSpecialFolderLocation(NULL, CSIDL_APPDATA, &pidl);
        if (hr != S_OK) {
            return false;
        }
        if (!pidl || !SHGetPathFromIDListW(pidl, szDocument)) {
            return false;
        }
        CoTaskMemFree(pidl);
        GetShortPathNameW(szDocument, m_lpszDefaultDir, _MAX_PATH);
        appdata_path = utf16To8(std::wstring(m_lpszDefaultDir));
        return true;
    };

    if (is_service) {
        if (!executeAsUser(get_path)) {
            return "";
        }
        return appdata_path;
    }
    if (!get_path(NULL)) {
        return "";
    }
    return appdata_path;
}

bool isRunasLocalSystem() {
    BOOL bIsLocalSystem = FALSE;
    PSID psidLocalSystem;
    SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;

    BOOL fSuccess = ::AllocateAndInitializeSid(&ntAuthority, 1, SECURITY_LOCAL_SYSTEM_RID, 0, 0, 0,
                                               0, 0, 0, 0, &psidLocalSystem);
    if (fSuccess) {
        fSuccess = ::CheckTokenMembership(0, psidLocalSystem, &bIsLocalSystem);
        ::FreeSid(psidLocalSystem);
    }

    return bIsLocalSystem;
}

bool isRunAsService() {
    DWORD current_process_id = GetCurrentProcessId();
    DWORD prev_session_id = 0;
    if (FALSE == ProcessIdToSessionId(current_process_id, &prev_session_id)) {
        return false;
    }
    return prev_session_id == 0;
}

int32_t getScreenWidth() {
    HDC hdc = GetDC(NULL);
    int32_t x = GetDeviceCaps(hdc, DESKTOPHORZRES);
    ReleaseDC(0, hdc);
    return x;
}

int32_t getScreenHeight() {
    HDC hdc = GetDC(NULL);
    int32_t y = GetDeviceCaps(hdc, DESKTOPVERTRES);
    ReleaseDC(0, hdc);
    return y;
}

DisplayOutputDesc getDisplayOutputDesc() {
    uint32_t width, height, frequency;
    DEVMODE dm;
    dm.dmSize = sizeof(DEVMODE);
    if (::EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, &dm)) {
        width = dm.dmPelsWidth;
        height = dm.dmPelsHeight;
        frequency = dm.dmDisplayFrequency ? dm.dmDisplayFrequency : 60;
    }
    else {
        width = getScreenWidth();
        height = getScreenHeight();
        frequency = 60;
    }
    return DisplayOutputDesc{width, height, frequency};
}

} // namespace ltlib