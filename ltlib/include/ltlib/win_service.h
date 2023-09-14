#pragma once
#include <ltlib/ltlib.h>
#include <cstdint>
#include <string>
#include <memory>
#include <Windows.h>

namespace ltlib
{

class LT_API WinApp
{
public:
    virtual ~WinApp() { }
    virtual void onStart() = 0;
    virtual void onStop() = 0;
    virtual void run() = 0;
};

class LT_API ServiceApp
{
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

class LT_API ServiceCtrl
{
public:
    static bool createService(const std::string& service_name, const std::string& display_name, const std::string& bin_path);
    static bool startService(const std::string& service_name);
    static bool stopService(const std::string& service_name);
    static bool isServiceRunning(const std::string& name, uint32_t& pid);
};

} // namespace ltlib
