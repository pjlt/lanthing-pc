#include "client_session.h"
#include <Windows.h>
#include <sstream>
#include <g3log/g3log.hpp>
#include <ltlib/strings.h>
#include <ltlib/system.h>

namespace
{

std::string to_string(ltrtc::VideoCodecType codec)
{
    switch (codec) {
    case ltrtc::VideoCodecType::H264:
        return "avc";
    case ltrtc::VideoCodecType::H265:
        return "hevc";
    default:
        return "unknown";
    }
}

} // 匿名空间

namespace lt
{

namespace ui
{

ClientSession::ClientSession(const Params& params)
    : params_(params)
{
}

ClientSession::~ClientSession()
{
    TerminateProcess(handle_, 0);
    CloseHandle(handle_);
}

bool ClientSession::start()
{
    // TODO: 改成跨平台的方式
    std::stringstream ss;
    ss << ltlib::get_program_path<char>() << "\\"
       << "lanthing.exe "
       << " -type client"
       << " -cid " << params_.client_id
       << " -rid " << params_.room_id
       << " -token " << params_.auth_token
       << " -user " << params_.p2p_username
       << " -pwd " << params_.p2p_password
       << " -addr " << params_.signaling_addr
       << " -port " << params_.signaling_port
       << " -codec " << to_string(params_.video_codec_type)
       << " -width " << params_.width
       << " -height " << params_.height
       << " -freq " << params_.refresh_rate
       << " -dinput " << (params_.enable_driver_input ? 1 : 0)
       << " -gamepad " << (params_.enable_gamepad ? 1 : 0);

    HANDLE token = NULL, user_token = NULL;
    HANDLE proc_handle = INVALID_HANDLE_VALUE;
    do {
        proc_handle = GetCurrentProcess();
        if (!OpenProcessToken(proc_handle, TOKEN_DUPLICATE, &token)) {
            break;
        }
        if (!DuplicateTokenEx(token, MAXIMUM_ALLOWED, 0, SecurityImpersonation, TokenPrimary, &user_token)) {
            break;
        }
        PROCESS_INFORMATION pi = { 0 };
        SECURITY_ATTRIBUTES sa = { 0 };
        sa.nLength = sizeof(SECURITY_ATTRIBUTES);
        STARTUPINFO si = { 0 };
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.cb = sizeof(STARTUPINFO);
        si.wShowWindow = SW_SHOW;

        LOG(INFO) << "Launching client: " << ss.str();
        std::wstring cmd = ltlib::utf8_to_utf16(ss.str());
        if (!CreateProcessAsUserW(user_token, NULL, const_cast<LPWSTR>(cmd.c_str()), &sa, &sa, FALSE, NORMAL_PRIORITY_CLASS, NULL, NULL, &si, &pi)) {
            break;
        }
        CloseHandle(token);
        CloseHandle(user_token);
        CloseHandle(pi.hThread);
        process_id_ = pi.dwProcessId;
        handle_ = pi.hProcess;
        std::promise<void> promise;
        auto future = promise.get_future();
        thread_ = ltlib::BlockingThread::create(
            "client_session", [&promise, this](const std::function<void()>& i_am_alive, void*) {
                promise.set_value();
                main_loop(i_am_alive);
            },
            nullptr);
        future.get();
        stoped_ = false;
        return true;
    } while (false);
    LOG(WARNING) << "Launch client failed: " << GetLastError();
    CloseHandle(token);
    if (user_token) {
        CloseHandle(user_token);
    }
    return false;
}

void ClientSession::main_loop(const std::function<void()>& i_am_alive)
{
    // HANDLE some_handle = ::CreateEventW(NULL, FALSE, FALSE, (L"Global\\lanthing_someevent_%lu" + std::to_wstring(process_id_)).c_str());
    constexpr uint32_t k500ms = 500;
    const HANDLE handles[] = { handle_ };
    while (true) {
        i_am_alive();
        auto ret = WaitForMultipleObjects(sizeof(handles) / sizeof(HANDLE), handles, FALSE, k500ms);
        switch (ret - WAIT_OBJECT_0) {
        case 0:
            stoped_ = true;
            params_.on_exited();
            return;
        default:
            break;
        }
    };
}

std::string ClientSession::client_id() const
{
    return params_.client_id;
}

} // namespace ui

} // namespace lt