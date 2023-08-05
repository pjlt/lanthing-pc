#include <ltlib/win_service.h>
#include <ltlib/strings.h>

namespace
{

ltlib::WinApp* g_app;
SERVICE_STATUS g_status;
SERVICE_STATUS_HANDLE g_status_handle;

bool get_service_status(SC_HANDLE service_handle, SERVICE_STATUS_PROCESS& service_status_process)
{
    DWORD bytes_needed = 0;
    if (::QueryServiceStatusEx(service_handle, SC_STATUS_PROCESS_INFO,
            (LPBYTE)&service_status_process, sizeof(service_status_process), &bytes_needed)) {
        return true;
    }
    return false;
}

} // namespace

namespace ltlib
{

ServiceApp::ServiceApp(WinApp* app)
//: service_name_ { service_name }
//, display_name_ { display_name }
//, bin_path_ { bin_path }
{
    g_app = app;
}

ServiceApp::~ServiceApp()
{
}

void ServiceApp::run()
{
    // 只能用脚本或其他进程提前创建好服务
    // if (create_service()) {
    run_service();
    //}
}

// bool ServiceApp::create_service()
//{
//     auto manager_handle = ::OpenSCManagerW(NULL, NULL, SC_MANAGER_ALL_ACCESS);
//     if (!manager_handle) {
//         return false;
//     }
//     std::wstring service_name = utf8_to_utf16(service_name_);
//     std::wstring display_name = utf8_to_utf16(display_name_);
//     std::wstring bin_path = utf8_to_utf16(bin_path_);
//     SC_HANDLE service_handle = ::CreateServiceW(
//         manager_handle,
//         service_name.c_str(),
//         display_name.c_str(),
//         SERVICE_ALL_ACCESS,
//         SERVICE_WIN32_OWN_PROCESS,
//         SERVICE_AUTO_START,
//         SERVICE_ERROR_NORMAL,
//         bin_path.c_str(),
//         NULL,
//         NULL,
//         NULL,
//         NULL,
//         NULL);
//     if (service_handle == NULL) {
//         ::CloseServiceHandle(manager_handle);
//         const DWORD error = GetLastError();
//         if (error == ERROR_SERVICE_EXISTS) {
//             return true;
//         } else {
//             return false;
//         }
//     }
//
//     SC_ACTION sc_action;
//     sc_action.Type = SC_ACTION_RESTART;
//     sc_action.Delay = 5000;
//     SERVICE_FAILURE_ACTIONS failure_action;
//     failure_action.dwResetPeriod = 0;
//     failure_action.lpRebootMsg = 0;
//     failure_action.lpCommand = 0;
//     failure_action.cActions = 1;
//     failure_action.lpsaActions = &sc_action;
//     ::ChangeServiceConfig2W(service_handle, SERVICE_CONFIG_FAILURE_ACTIONS, &failure_action);
//     ::CloseServiceHandle(service_handle);
//     ::CloseServiceHandle(manager_handle);
//     return true;
// }

void ServiceApp::run_service()
{
    wchar_t service_name[1024] = { 0 };
    SERVICE_TABLE_ENTRYW dispatch_table[] = {
        { service_name, (LPSERVICE_MAIN_FUNCTIONW)service_main },
        { NULL, NULL }
    };
    ::StartServiceCtrlDispatcherW(dispatch_table);
    // DWORD error = GetLastError();
    //  printf("ret:%d, error:%d", ret, error);
}

bool ServiceApp::report_status(uint32_t current_state, uint32_t win32_exit_code, uint32_t wait_hint)
{
    if (current_state == SERVICE_START_PENDING)
        g_status.dwControlsAccepted = 0;
    else
        g_status.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;

    g_status.dwCurrentState = current_state;
    g_status.dwWin32ExitCode = win32_exit_code;
    g_status.dwWaitHint = wait_hint;

    if ((current_state == SERVICE_RUNNING) || (current_state == SERVICE_STOPPED))
        g_status.dwCheckPoint = 0;
    else
        g_status.dwCheckPoint++;

    return SetServiceStatus(g_status_handle, &g_status) == TRUE;
}

void __stdcall ServiceApp::service_main()
{
    std::wstring service_name;
    g_status_handle = ::RegisterServiceCtrlHandlerW(service_name.c_str(), &service_control_handler);
    g_status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    g_status.dwServiceSpecificExitCode = 0;
    report_status(SERVICE_START_PENDING, NO_ERROR, 0);
    report_status(SERVICE_RUNNING, NO_ERROR, 0);
    g_app->on_start();
    g_app->run();
    report_status(SERVICE_STOPPED, NO_ERROR, 0);
}

void __stdcall ServiceApp::service_control_handler(unsigned long ctrl_code)
{
    switch (ctrl_code) {
    case SERVICE_CONTROL_STOP:
    case SERVICE_CONTROL_SHUTDOWN:
        report_status(SERVICE_STOP_PENDING, NO_ERROR, 0);
        g_app->on_stop();
        break;
    default:
        break;
    }
}

bool is_service_running(const std::string& _name, uint32_t& pid)
{
    std::wstring name = utf8_to_utf16(_name);
    auto manager_handle = ::OpenSCManagerW(NULL, NULL, SC_MANAGER_QUERY_LOCK_STATUS);
    if (!manager_handle) {
        return false;
    }
    SC_HANDLE service_handle = ::OpenServiceW(manager_handle, name.c_str(),
        SERVICE_QUERY_STATUS);
    if (service_handle == NULL) {
        ::CloseServiceHandle(manager_handle);
        return false;
    }
    SERVICE_STATUS_PROCESS status { 0 };
    get_service_status(service_handle, status);
    ::CloseServiceHandle(service_handle);
    ::CloseServiceHandle(manager_handle);
    pid = status.dwProcessId;
    return (status.dwCurrentState == SERVICE_RUNNING);
}

} // namespace ltlib
