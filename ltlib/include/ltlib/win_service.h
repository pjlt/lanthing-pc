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

#pragma once
#include <Windows.h>

#include <cstdint>
#include <memory>
#include <string>

namespace ltlib {

class WinApp {
public:
    virtual ~WinApp() {}
    virtual void onStart() = 0;
    virtual void onStop() = 0;
    virtual void run() = 0;
};

class ServiceApp {
public:
    ServiceApp(WinApp* service);
    ~ServiceApp();
    void run();

private:
    static bool reportStatus(uint32_t current_state, uint32_t win32_exit_code, uint32_t wait_hint);
    static void __stdcall serviceMain();
    static void __stdcall serviceControlHandler(unsigned long ctrl_code);
    void run_service();
};

class ServiceCtrl {
public:
    static bool createService(const std::string& service_name, const std::string& display_name,
                              const std::string& bin_path);
    static bool startService(const std::string& service_name);
    static bool stopService(const std::string& service_name);
    static bool isServiceRunning(const std::string& name, uint32_t& pid);
};

} // namespace ltlib
