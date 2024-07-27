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

#include <ltlib/strings.h>
#include <ltlib/win_service.h>

namespace {

ltlib::WinApp* g_app;
SERVICE_STATUS g_status;
SERVICE_STATUS_HANDLE g_status_handle;

bool get_service_status(SC_HANDLE service_handle, SERVICE_STATUS_PROCESS& service_status_process) {
    DWORD bytes_needed = 0;
    if (::QueryServiceStatusEx(service_handle, SC_STATUS_PROCESS_INFO,
                               (LPBYTE)&service_status_process, sizeof(service_status_process),
                               &bytes_needed)) {
        return true;
    }
    return false;
}

} // namespace

namespace ltlib {

ServiceApp::ServiceApp(WinApp* app)
//: service_name_ { service_name }
//, display_name_ { display_name }
//, bin_path_ { bin_path }
{
    g_app = app;
}

ServiceApp::~ServiceApp() {}

void ServiceApp::run() {
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
//     std::wstring service_name = utf8To16(service_name_);
//     std::wstring display_name = utf8To16(display_name_);
//     std::wstring bin_path = utf8To16(bin_path_);
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

void ServiceApp::run_service() {
    wchar_t service_name[1024] = {0};
    SERVICE_TABLE_ENTRYW dispatch_table[] = {{service_name, (LPSERVICE_MAIN_FUNCTIONW)serviceMain},
                                             {NULL, NULL}};
    ::StartServiceCtrlDispatcherW(dispatch_table);
    // DWORD error = GetLastError();
    //  printf("ret:%d, error:%d", ret, error);
}

bool ServiceApp::reportStatus(uint32_t current_state, uint32_t win32_exit_code,
                              uint32_t wait_hint) {
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

void __stdcall ServiceApp::serviceMain() {
    std::wstring service_name;
    g_status_handle = ::RegisterServiceCtrlHandlerW(service_name.c_str(), &serviceControlHandler);
    g_status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    g_status.dwServiceSpecificExitCode = 0;
    reportStatus(SERVICE_START_PENDING, NO_ERROR, 0);
    reportStatus(SERVICE_RUNNING, NO_ERROR, 0);
    g_app->onStart();
    g_app->run();
    reportStatus(SERVICE_STOPPED, NO_ERROR, 0);
}

void __stdcall ServiceApp::serviceControlHandler(unsigned long ctrl_code) {
    switch (ctrl_code) {
    case SERVICE_CONTROL_STOP:
    case SERVICE_CONTROL_SHUTDOWN:
        reportStatus(SERVICE_STOP_PENDING, NO_ERROR, 0);
        g_app->onStop();
        break;
    default:
        break;
    }
}

static bool maybeChangeBinaryPath(SC_HANDLE manager_handle, const std::wstring& service_name,
                                  const std::wstring& display_name, const std::wstring& bin_path) {
    SC_HANDLE service_handle =
        ::OpenServiceW(manager_handle, service_name.c_str(), SERVICE_ALL_ACCESS);
    if (service_handle == NULL) {
        ::CloseServiceHandle(manager_handle);
        return false;
    }
    BOOL success = ChangeServiceConfigW(service_handle, SERVICE_WIN32_OWN_PROCESS,
                                        SERVICE_AUTO_START, SERVICE_ERROR_NORMAL, bin_path.c_str(),
                                        NULL, NULL, NULL, NULL, NULL, display_name.c_str());

    ::CloseServiceHandle(manager_handle);
    ::CloseServiceHandle(service_handle);
    return success == TRUE;
}

bool ServiceCtrl::createService(const std::string& _service_name, const std::string& _display_name,
                                const std::string& _bin_path) {
    auto manager_handle = ::OpenSCManagerW(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (!manager_handle) {
        return false;
    }
    std::wstring service_name = utf8To16(_service_name);
    std::wstring display_name = utf8To16(_display_name);
    std::wstring bin_path = utf8To16(_bin_path);
    SC_HANDLE service_handle =
        ::CreateServiceW(manager_handle, service_name.c_str(), display_name.c_str(),
                         SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS, SERVICE_AUTO_START,
                         SERVICE_ERROR_NORMAL, bin_path.c_str(), NULL, NULL, NULL, NULL, NULL);
    if (service_handle == NULL) {
        const DWORD error = GetLastError();
        if (error == ERROR_SERVICE_EXISTS) {
            return maybeChangeBinaryPath(manager_handle, service_name, display_name, bin_path);
        }
        else {
            ::CloseServiceHandle(manager_handle);
            return false;
        }
    }

    SC_ACTION sc_action;
    sc_action.Type = SC_ACTION_RESTART;
    sc_action.Delay = 5000;
    SERVICE_FAILURE_ACTIONS failure_action;
    failure_action.dwResetPeriod = 0;
    failure_action.lpRebootMsg = 0;
    failure_action.lpCommand = 0;
    failure_action.cActions = 1;
    failure_action.lpsaActions = &sc_action;
    ::ChangeServiceConfig2W(service_handle, SERVICE_CONFIG_FAILURE_ACTIONS, &failure_action);
    ::CloseServiceHandle(service_handle);
    ::CloseServiceHandle(manager_handle);
    return true;
}

bool ServiceCtrl::startService(const std::string& _service_name) {
    auto manager_handle = ::OpenSCManagerW(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (!manager_handle) {
        return false;
    }
    std::wstring service_name = utf8To16(_service_name);
    SC_HANDLE service_handle =
        ::OpenServiceW(manager_handle, service_name.c_str(), SERVICE_ALL_ACCESS);
    ::CloseServiceHandle(manager_handle);
    if (service_handle == NULL) {
        return false;
    }
    BOOL success = ::StartServiceW(service_handle, 0, NULL);
    ::CloseServiceHandle(service_handle);
    return success;
}

bool ServiceCtrl::stopService(const std::string& _service_name) {
    auto manager_handle = ::OpenSCManagerW(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (!manager_handle) {
        return false;
    }
    std::wstring service_name = utf8To16(_service_name);
    SC_HANDLE service_handle =
        ::OpenServiceW(manager_handle, service_name.c_str(), SERVICE_ALL_ACCESS);
    ::CloseServiceHandle(manager_handle);
    if (service_handle == NULL) {
        return false;
    }
    SERVICE_STATUS status;
    BOOL success = ::ControlService(service_handle, SERVICE_CONTROL_STOP, &status);
    ::CloseServiceHandle(service_handle);
    return success;
}

bool ServiceCtrl::isServiceRunning(const std::string& _name, uint32_t& pid) {
    std::wstring name = utf8To16(_name);
    auto manager_handle = ::OpenSCManagerW(NULL, NULL, SC_MANAGER_QUERY_LOCK_STATUS);
    if (!manager_handle) {
        return false;
    }
    SC_HANDLE service_handle = ::OpenServiceW(manager_handle, name.c_str(), SERVICE_QUERY_STATUS);
    if (service_handle == NULL) {
        ::CloseServiceHandle(manager_handle);
        return false;
    }
    SERVICE_STATUS_PROCESS status{0};
    get_service_status(service_handle, status);
    ::CloseServiceHandle(service_handle);
    ::CloseServiceHandle(manager_handle);
    pid = status.dwProcessId;
    return (status.dwCurrentState == SERVICE_RUNNING);
}

} // namespace ltlib
