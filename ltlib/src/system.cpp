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

#if LT_WINDOWS
#include <Windows.h>

#include <Shlobj.h>
#include <TlHelp32.h>
#include <shellapi.h>
#elif LT_LINUX
#include <X11/Xlib.h>
#include <pwd.h>
#include <unistd.h>
#else
#endif // LT_WINDOWS

#include <climits>
#include <cstring>

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

#include <ltlib/logging.h>
#include <ltlib/strings.h>
#include <ltlib/system.h>

namespace {

#if defined(LT_WINDOWS)

struct AutoGuard {
    AutoGuard(const std::function<void()>& func)
        : func_{func} {}
    ~AutoGuard() {
        if (func_) {
            func_();
        }
    }

private:
    std::function<void()> func_;
};

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

bool processNotElevated() {
    bool isElevated = false;
    HANDLE token = NULL;

    BOOL ret = OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token);
    if (ret == FALSE) {
        return false;
    }

    TOKEN_ELEVATION elevationInfo;
    DWORD retSize = 0;
    if (!GetTokenInformation(token, TokenElevation, &elevationInfo, sizeof(elevationInfo),
                             &retSize)) {
        CloseHandle(token);
        return false;
    }

    isElevated = elevationInfo.TokenIsElevated;
    CloseHandle(token);
    return !isElevated;
}

#endif // LT_WINDOWS

} // namespace

namespace ltlib {

#if defined(LT_WINDOWS)

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

std::string getConfigPath(bool is_service) {
    static std::string appdata_path;
    if (!appdata_path.empty()) {
        return appdata_path;
    }

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
        std::filesystem::path fs = appdata_path;
        fs = fs / "lanthing";
        appdata_path = fs.string();
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

DisplayOutputDesc getDisplayOutputDesc(const std::string& name) {
    uint32_t width, height, frequency, rotation;
    DEVMODE dm{};
    dm.dmSize = sizeof(DEVMODE);
    if (::EnumDisplaySettingsA(name.empty() ? nullptr : name.c_str(), ENUM_CURRENT_SETTINGS, &dm)) {
        frequency = dm.dmDisplayFrequency ? dm.dmDisplayFrequency : 60;
        switch (dm.dmDisplayOrientation) {
        case DMDO_DEFAULT:
            rotation = 0;
            break;
        case DMDO_90:
            rotation = 90;
            break;
        case DMDO_180:
            rotation = 180;
            break;
        case DMDO_270:
            rotation = 270;
            break;
        default:
            rotation = 0;
            break;
        }
        width = dm.dmPelsWidth;
        height = dm.dmPelsHeight;
    }
    else {
        // 哪个屏幕?
        width = getScreenWidth();
        height = getScreenHeight();
        frequency = 60;
        rotation = 0;
    }
    return DisplayOutputDesc{width, height, frequency, rotation};
}

bool changeDisplaySettings(uint32_t w, uint32_t h, uint32_t f) {
    DEVMODE dm{};
    dm.dmSize = sizeof(DEVMODE);
    if (!::EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, &dm)) {
        return false;
    }
    dm.dmFields = 0;
    if (dm.dmPelsHeight != h) {
        dm.dmFields |= DM_PELSHEIGHT;
    }
    if (dm.dmPelsWidth != w) {
        dm.dmFields |= DM_PELSWIDTH;
    }
    if (dm.dmDisplayFrequency != f) {
        dm.dmFields |= DM_DISPLAYFREQUENCY;
    }
    dm.dmPelsHeight = h;
    dm.dmPelsWidth = w;
    dm.dmDisplayFrequency = f;
    auto ret = ::ChangeDisplaySettings(&dm, 0);
    if (ret != DISP_CHANGE_SUCCESSFUL) {
        dm.dmFields = DM_PELSHEIGHT | DM_PELSWIDTH;
        ret = ::ChangeDisplaySettings(&dm, 0);
        if (ret != DISP_CHANGE_SUCCESSFUL) {
            dm.dmFields = DM_DISPLAYFREQUENCY;
            ret = ::ChangeDisplaySettings(&dm, 0);
            if (ret != DISP_CHANGE_SUCCESSFUL) {
                return false;
            }
        }
    }
    return true;
}

bool setThreadDesktop() {
    wchar_t w_current_name[512] = {0};
    wchar_t w_input_name[512] = {0};
    HDESK current_desktop = GetThreadDesktop(GetCurrentThreadId());
    if (current_desktop == nullptr) {
        LOGF(ERR, "GetThreadDesktop(GetCurrentThreadId()) failed with %#x", GetLastError());
        return false;
    }
    AutoGuard g1{[&]() { CloseDesktop(current_desktop); }};
    HDESK input_desktop = OpenInputDesktop(0, FALSE, GENERIC_ALL);
    if (input_desktop == nullptr) {
        LOGF(ERR, "OpenInputDesktop failed with %#x", GetLastError());
        return false;
    }
    AutoGuard g2{[&]() { CloseDesktop(input_desktop); }};
    DWORD size = 0;
    BOOL success = GetUserObjectInformationW(current_desktop, UOI_NAME, w_current_name,
                                             sizeof(w_current_name), &size);
    if (success == FALSE) {
        LOGF(ERR, "GetUserObjectInformationW(current_desktop) failed with %#x", GetLastError());
        return false;
    }
    success = GetUserObjectInformationW(input_desktop, UOI_NAME, w_input_name, sizeof(w_input_name),
                                        &size);
    if (success == FALSE) {
        LOGF(ERR, "GetUserObjectInformationW(current_desktop) failed with %#x", GetLastError());
        return false;
    }
    std::wstring current_name = w_current_name;
    std::wstring input_name = w_input_name;
    if (current_name != input_name) {
        success = SetThreadDesktop(input_desktop);
        if (success == FALSE) {
            LOGF(ERR, "SetThreadDesktop failed with %#x", GetLastError());
            return false;
        }
        else {
            LOG(INFO) << "SetThreadDesktop success";
        }
    }
    else {
        LOG(INFO) << "No need for SetThreadDesktop";
    }
    return true;
}

bool selfElevateAndNeedExit() {
    // 明确是非管理员启动、且成功执行ShellExecuteEx才返回true
    if (processNotElevated()) {
        std::string fullpath = getProgramFullpath();
        if (fullpath.empty()) {
            return false;
        }
        SHELLEXECUTEINFO sei{};
        sei.cbSize = sizeof(SHELLEXECUTEINFO);
        sei.lpVerb = "runas";
        sei.lpFile = fullpath.c_str();
        sei.hwnd = NULL;
        sei.nShow = SW_NORMAL;
        if (!ShellExecuteExA(&sei)) {
            return false;
        }
        return true;
    }
    return false;
}

std::vector<Monitor> enumMonitors() {
    std::vector<Monitor> monitors;
    monitors.push_back(Monitor{});
    auto func = [](HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData) -> BOOL {
        (void)hdcMonitor;
        (void)lprcMonitor;
        auto monitors = reinterpret_cast<std::vector<Monitor>*>(dwData);
        MONITORINFOEXA info{};
        info.cbSize = sizeof(MONITORINFOEXA);
        GetMonitorInfoA(hMonitor, &info);
        if (info.dwFlags == DISPLAY_DEVICE_MIRRORING_DRIVER) {
            return TRUE;
        }
        DEVMODE mode{};
        int32_t rotation = 0;
        if (EnumDisplaySettingsA(info.szDevice, ENUM_CURRENT_SETTINGS, &mode)) {
            switch (mode.dmDisplayOrientation) {
            case DMDO_DEFAULT:
                rotation = 0;
                break;
            case DMDO_90:
                rotation = 90;
                break;
            case DMDO_180:
                rotation = 180;
                break;
            case DMDO_270:
                rotation = 270;
                break;
            default:
                rotation = 0;
                break;
            }
        }
        auto frequency = mode.dmDisplayFrequency ? mode.dmDisplayFrequency : 60;
        char buff[CCHDEVICENAME + 1] = {0};
        memcpy(buff, info.szDevice, CCHDEVICENAME);
        std::string name = buff;
        if (info.dwFlags == MONITORINFOF_PRIMARY) {
            monitors->operator[](0) = Monitor{info.rcMonitor.left,
                                              info.rcMonitor.top,
                                              info.rcMonitor.right,
                                              info.rcMonitor.bottom,
                                              static_cast<int32_t>(mode.dmPelsWidth),
                                              static_cast<int32_t>(mode.dmPelsHeight),
                                              rotation,
                                              static_cast<int32_t>(frequency),
                                              name};
        }
        else {
            monitors->push_back(Monitor{info.rcMonitor.left, info.rcMonitor.top,
                                        info.rcMonitor.right, info.rcMonitor.bottom,
                                        static_cast<int32_t>(mode.dmPelsWidth),
                                        static_cast<int32_t>(mode.dmPelsHeight), rotation,
                                        static_cast<int32_t>(frequency), name});
        }
        return TRUE;
    };
    if (!EnumDisplayMonitors(nullptr, nullptr, func, reinterpret_cast<LPARAM>(&monitors))) {
        LOGF(ERR, "EnumDisplayMonitors failed with %#x", GetLastError()); // MSDN没说GetLastError
        return {};
    }
    if (monitors[0].width == 0 || monitors[0].height == 0) {
        LOGF(ERR, "EnumDisplayMonitors failed, primary monitor is zero");
        return {};
    }
    return monitors;
}

void openFolder(const std::string& path) {
    HRESULT hr = CoInitializeEx(0, COINIT_APARTMENTTHREADED);
    if (hr != RPC_E_CHANGED_MODE) {
        if (FAILED(hr)) {
            LOG(WARNING) << "openFolder: CoInitializeEx failed";
            return;
        }
    }
    HINSTANCE ret = ShellExecuteA(nullptr, "open", path.c_str(), nullptr, nullptr, SW_SHOWDEFAULT);
    if (reinterpret_cast<int64_t>(ret) > 32) {
        LOG(INFO) << "openFolder " << path << " success";
    }
    else {
        LOG(ERR) << "openFolder " << path << " failed, ret " << reinterpret_cast<int64_t>(ret)
                 << ", GetLastError() " << GetLastError();
    }
    CoUninitialize();
}

#elif defined(LT_LINUX)

std::string getProgramFullpath() {
    char result[PATH_MAX];
    auto count = readlink("/proc/self/exe", result, PATH_MAX);
    if (count <= 0) {
        return "";
    }
    return std::string(result, result + count);
}

std::string getProgramPath() {
    std::string fullpath = getProgramFullpath();
    if (fullpath.empty()) {
        return "";
    }
    auto pos = fullpath.rfind('/');
    if (pos != std::string::npos) {
        return fullpath.substr(0, pos);
    }
    return "";
}

std::string getConfigPath(bool is_service) {
    (void)is_service;
    static std::string config_path;
    if (!config_path.empty()) {
        return config_path;
    }
    std::filesystem::path fs = getpwuid(getuid())->pw_dir;
    fs = fs / ".lanthing";
    config_path = fs.string();
    return config_path;
}

bool isRunasLocalSystem() {
    return false;
}
bool isRunAsService() {
    return false;
}

int32_t getScreenWidth() {
    return -1;
}
int32_t getScreenHeight() {
    return -1;
}

DisplayOutputDesc getDisplayOutputDesc(const std::string& name) {
    // 选择屏幕?
    (void)name;
    Display* d = XOpenDisplay(nullptr);
    if (d == nullptr) {
        return {0, 0, 0, 0};
    }
    Screen* s = DefaultScreenOfDisplay(d);
    if (s == nullptr) {
        return {0, 0, 0, 0};
    }
    uint32_t width = s->width;
    uint32_t height = s->height;
    XCloseDisplay(d);
    return {width, height, 60, 0};
}

bool changeDisplaySettings(uint32_t w, uint32_t h, uint32_t f) {
    (void)w;
    (void)h;
    (void)f;
    return false;
}

bool setThreadDesktop() {
    return false;
}

bool selfElevateAndNeedExit() {
    return false;
}

std::vector<Monitor> enumMonitors() {
    return {};
}

void openFolder(const std::string& path) {
    (void)path;
}

#endif // #elif defined(LT_LINUX)

} // namespace ltlib