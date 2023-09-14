#pragma once
#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <thread>

namespace lt {

namespace worker {

class SessionChangeObserver {
public:
    static std::unique_ptr<SessionChangeObserver> create();
    void waitForChange();
    void stop();

private:
    SessionChangeObserver() = default;
    void waitingLoop();

private:
    // std::thread thread_;
    std::atomic<bool> stoped_{false};
    uint32_t startup_session_id_;
    std::wstring startup_desk_name_;
};

} // namespace worker

} // namespace lt