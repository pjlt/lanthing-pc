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

#include <g3log/g3log.hpp>

#include "daemon_win.h"
#include <service/service.h>

namespace lt {

namespace svc {

HANDLE g_stop_service_handle = NULL;

LanthingWinService::LanthingWinService()
    : impl_(std::make_unique<Service>()) {}

LanthingWinService::~LanthingWinService() {}

void LanthingWinService::onStart() {
    if (!impl_->init()) {
        is_stop_ = true;
    }
    return;
}

void LanthingWinService::onStop() {
    LOG(INFO) << "Lanthing service on stop";
    is_stop_ = true;
    if (g_stop_service_handle) {
        SetEvent(g_stop_service_handle);
        LOG(INFO) << "Emit service exit event";
    }
    // 不清楚Windows服务on_stop的行为，所以主动调uninit()而不是放到impl的析构函数里
    impl_->uninit();
}

void LanthingWinService::run() {
    std::wstringstream ss;
    ss << L"Global\\lanthing_stop_service_" << GetCurrentProcessId();
    std::wstring event_name = ss.str();

    PSECURITY_DESCRIPTOR psd = NULL;
    BYTE sd[SECURITY_DESCRIPTOR_MIN_LENGTH];
    psd = (PSECURITY_DESCRIPTOR)sd;
    InitializeSecurityDescriptor(psd, SECURITY_DESCRIPTOR_REVISION);
    SetSecurityDescriptorDacl(psd, TRUE, (PACL)NULL, FALSE);
    SECURITY_ATTRIBUTES sa = {sizeof(sa), psd, FALSE};
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