//#include <process.h>
#include <Windows.h>
#include <TlHelp32.h>
#include <Shlobj.h>
#include <string>
#include <functional>
#include <vector>
#include <ltlib/system.h>
#include <ltlib/strings.h>

namespace
{

BOOL GetTokenByName(HANDLE& hToken, const LPTSTR lpName)
{
    if (!lpName)
        return FALSE;

    HANDLE hProcessSnap = NULL;
    BOOL bRet = FALSE;
    PROCESSENTRY32W pe32 = { 0 };

    hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hProcessSnap == INVALID_HANDLE_VALUE)
        return (FALSE);

    pe32.dwSize = sizeof(PROCESSENTRY32W);

    if (Process32First(hProcessSnap, &pe32)) {
        do {
            if (!wcscmp(wcsupr(pe32.szExeFile), wcsupr(lpName))) {
                HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pe32.th32ProcessID);
                bRet = OpenProcessToken(hProcess, TOKEN_ALL_ACCESS, &hToken);
                CloseHandle(hProcess);
                CloseHandle(hProcessSnap);
                return (bRet);
            }
        } while (Process32NextW(hProcessSnap, &pe32));
        bRet = FALSE;
    } else {
        bRet = FALSE;
    }

    CloseHandle(hProcessSnap);
    return (bRet);
}

bool execute_as_user(const std::function<bool(HANDLE)>& func)
{
    HANDLE hToken = NULL;
    bool res = false;
    do {
        //可能有些电脑在刚开机一直会失败
        if (!GetTokenByName(hToken, (const LPTSTR) L"EXPLORER.EXE")) {
            return false;
        }
        ////可能进程还没起来
        //for (int i = 0; i < 1; i++)
        //{
        //    if (!GetTokenByName(hToken, (const LPTSTR)_T("EXPLORER.EXE")))
        //    {
        //        std::this_thread::sleep_for(std::chrono::seconds(1));
        //        continue;
        //    }
        //    break;
        //}

        if (!hToken) {
            break;
        }

        // 模拟登录用户的安全上下文
        if (FALSE == ImpersonateLoggedOnUser(hToken)) {
            break;
        }

        res = func(hToken);

        // 到这里已经模拟完了，别忘记返回原来的安全上下文
        if (FALSE == RevertToSelf()) {
            break;
        }
    } while (0);

    if (NULL != hToken) {
        CloseHandle(hToken);
    }
    return res;
}

} // 匿名空间

namespace ltlib
{

bool get_program_filename(std::string& filename)
{
    char the_filename[MAX_PATH];
    DWORD length = ::GetModuleFileNameA(nullptr, the_filename, MAX_PATH);
    if (length > 0 && length < MAX_PATH) {
        filename = the_filename;
        return true;
    }
    return false;
}

bool get_program_filename(std::wstring& filename)
{
    const int kMaxPath = UNICODE_STRING_MAX_CHARS; // unicode支持32767
    std::vector<wchar_t> the_filename(kMaxPath);
    DWORD length = ::GetModuleFileNameW(nullptr, the_filename.data(), kMaxPath);
    if (length > 0 && length < kMaxPath) {
        filename = std::wstring(the_filename.data(), length);
        return true;
    }
    return false;
}

bool get_program_path(std::string& path)
{
    std::string filename;
    if (get_program_filename(filename)) {
        std::string::size_type pos = filename.rfind('\\');
        if (pos != std::string::npos) {
            path = filename.substr(0, pos);
            return true;
        }
    }
    return false;
}

bool get_program_path(std::wstring& path)
{
    std::wstring filename;
    if (get_program_filename(filename)) {
        std::wstring::size_type pos = filename.rfind(L'\\');
        if (pos != std::wstring::npos) {
            path = filename.substr(0, pos);
            return true;
        }
    }
    return false;
}

template<>
std::string get_program_path<char>()
{
    std::string path;
    get_program_path(path);
    return path;
}

template <>
std::wstring get_program_path<wchar_t>()
{
    std::wstring path;
    get_program_path(path);
    return path;
}

template <>
std::string get_program_fullpath<char>()
{
    std::string path;
    get_program_filename(path);
    return path;
}

template <>
std::wstring get_program_fullpath<wchar_t>()
{
    std::wstring path;
    get_program_filename(path);
    return path;
}

uint32_t get_session_id_by_pid(uint32_t pid)
{
    DWORD sid = 0;
    if (FALSE == ProcessIdToSessionId(pid, &sid)) {
        //TODO error handling
        return 0;
    }
    return sid;
}

uint32_t get_parent_pid(uint32_t curr_pid)
{
    HANDLE PHANDLE = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);
    if (PHANDLE == INVALID_HANDLE_VALUE) {
        return 0;
    }

    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(pe32);
    pe32.dwFlags = sizeof(pe32);
    BOOL hProcess = Process32FirstW(PHANDLE, &pe32);

    while (hProcess) {
        if (pe32.th32ProcessID == curr_pid) {
            CloseHandle(PHANDLE);
            return pe32.th32ParentProcessID;
        }
        hProcess = Process32NextW(PHANDLE, &pe32);
    }

    CloseHandle(PHANDLE);
    return 0;
}

std::string get_appdata_path(bool is_service)
{
    static std::string appdata_path;
    if (!appdata_path.empty()) {
        return appdata_path;
    }
    std::wstring wappdata_path;

    auto get_path = [&](HANDLE) -> bool {
        wchar_t m_lpszDefaultDir[MAX_PATH] = { 0 };
        wchar_t szDocument[MAX_PATH] = { 0 };

        LPITEMIDLIST pidl = NULL;
        auto hr = SHGetSpecialFolderLocation(NULL, CSIDL_APPDATA, &pidl);
        if (hr != S_OK) {
            return false;
        }
        if (!pidl || !SHGetPathFromIDList(pidl, szDocument)) {
            return false;
        }
        CoTaskMemFree(pidl);
        GetShortPathName(szDocument, m_lpszDefaultDir, _MAX_PATH);
        appdata_path =  utf16_to_utf8(std::wstring(m_lpszDefaultDir));
        return true;
    };

    if (is_service) {
        if (!execute_as_user(get_path)) {
            //log_print(kError, _T("get_path fail"));
            return "";
        }
        return appdata_path;
    }
    if (!get_path(NULL)) {
        return "";
    }
    return appdata_path;
}

bool is_run_as_local_system()
{
    BOOL bIsLocalSystem = FALSE;
    PSID psidLocalSystem;
    SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;

    BOOL fSuccess = ::AllocateAndInitializeSid(&ntAuthority, 1, SECURITY_LOCAL_SYSTEM_RID,
        0, 0, 0, 0, 0, 0, 0, &psidLocalSystem);
    if (fSuccess) {
        fSuccess = ::CheckTokenMembership(0, psidLocalSystem, &bIsLocalSystem);
        ::FreeSid(psidLocalSystem);
    }

    return bIsLocalSystem;
}

bool is_run_as_service()
{
    DWORD current_process_id = GetCurrentProcessId();
    DWORD prev_session_id = 0;
    if (FALSE == ProcessIdToSessionId(current_process_id, &prev_session_id)) {
        return false;
    }
    return prev_session_id == 0;
}

int32_t get_screen_width()
{
    HDC hdc = GetDC(NULL);
    int32_t x = GetDeviceCaps(hdc, DESKTOPHORZRES);
    ReleaseDC(0, hdc);
    return x;
}

int32_t get_screen_height()
{
    HDC hdc = GetDC(NULL);
    int32_t y = GetDeviceCaps(hdc, DESKTOPVERTRES);
    ReleaseDC(0, hdc);
    return y;
}

bool set_thread_desktop()
{
    bool succes = false;
    HDESK thread_desktop = nullptr;
    HDESK input_desktop = nullptr;

    do {

        DWORD current_thread_id = GetCurrentThreadId();
        thread_desktop = GetThreadDesktop(current_thread_id);

        input_desktop = OpenInputDesktop(0, TRUE, DESKTOP_CREATEMENU | DESKTOP_CREATEWINDOW | DESKTOP_ENUMERATE | DESKTOP_HOOKCONTROL | DESKTOP_WRITEOBJECTS | DESKTOP_READOBJECTS | DESKTOP_SWITCHDESKTOP | GENERIC_WRITE);

        if (!input_desktop) {
            break;
        }

        TCHAR cur_thread_desktop_name[1024] = { 0 };
        TCHAR cur_input_desktop_name[1024] = { 0 };
        DWORD needLength = 0;
        if (!GetUserObjectInformation(thread_desktop, UOI_NAME, cur_thread_desktop_name, sizeof(cur_thread_desktop_name), &needLength)) {
            break;
        }
        if (!GetUserObjectInformation(input_desktop, UOI_NAME, cur_input_desktop_name, sizeof(cur_input_desktop_name), &needLength)) {
            break;
        }

        std::wstring input_desktop_name;
        input_desktop_name = cur_input_desktop_name;

        if (input_desktop_name != cur_thread_desktop_name) {
            if (!SetThreadDesktop(input_desktop)) {
                break;
            }
            succes = true;
        } else {
            succes = true;
        }
    } while (0);

    if (thread_desktop) {
        CloseDesktop(thread_desktop);
    }

    if (input_desktop) {
        CloseDesktop(input_desktop);
    }

    return succes;
}

DisplayOutputDesc get_display_output_desc()
{
    uint32_t width, height, frequency;
    DEVMODE dm;
    dm.dmSize = sizeof(DEVMODE);
    if (::EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, &dm)) {
        width = dm.dmPelsWidth;
        height = dm.dmPelsHeight;
        frequency = dm.dmDisplayFrequency ? dm.dmDisplayFrequency : 60;
    } else {
        width = get_screen_width();
        height = get_screen_height();
        frequency = 60;
    }
    return DisplayOutputDesc { width, height, frequency };
}

} // namespace ltlib