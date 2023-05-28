#include <g3log/g3log.hpp>
#include "daemon_win.h"
#include <service/service.h>

namespace lt
{

namespace svc
{

HANDLE g_stop_service_handle = NULL;

LanthingWinService::LanthingWinService()
    : impl_(std::make_unique<Service>())
{
}

LanthingWinService::~LanthingWinService()
{
}

void LanthingWinService::on_start()
{
    if (!impl_->init()) {
        is_stop_ = true;
    }
    return;
}

void LanthingWinService::on_stop()
{
    LOG(INFO) << "Lanthing service on stop";
    is_stop_ = true;
    if (g_stop_service_handle) {
        SetEvent(g_stop_service_handle);
        LOG(INFO) << "Emit service exit event";
    }
    // 不清楚Windows服务on_stop的行为，所以主动调uninit()而不是放到impl的析构函数里
    impl_->uninit();
}

void LanthingWinService::run()
{
    std::wstringstream ss;
    ss << L"Global\\lanthing_stop_service_" << GetCurrentProcessId();
    std::wstring event_name = ss.str();

    PSECURITY_DESCRIPTOR psd = NULL;
    BYTE sd[SECURITY_DESCRIPTOR_MIN_LENGTH];
    psd = (PSECURITY_DESCRIPTOR)sd;
    InitializeSecurityDescriptor(psd, SECURITY_DESCRIPTOR_REVISION);
    SetSecurityDescriptorDacl(psd, TRUE, (PACL)NULL, FALSE);
    SECURITY_ATTRIBUTES sa = { sizeof(sa), psd, FALSE };
    g_stop_service_handle = ::CreateEventW(&sa, FALSE, FALSE, event_name.c_str());

    if (!g_stop_service_handle) {
        LOG(WARNING) << "Create lanthing stop event failed: " << GetLastError();
        return;
    }

    LOG(INFO) << "Lanthing started";
    while (!is_stop_) {
        DWORD ret = WaitForSingleObject(g_stop_service_handle, 1000);
        if (ret == WAIT_TIMEOUT) {
            continue;
        }
        LOG(INFO) << "WaitForSingleObject(lanthing_stop_event), return: " << ret;
        is_stop_ = true;
    }
    LOG(INFO) << "Lanthing service exit";
    CloseHandle(g_stop_service_handle);
    g_stop_service_handle = nullptr;
    impl_->uninit();
}

} // namespace svc

} // namespace lt