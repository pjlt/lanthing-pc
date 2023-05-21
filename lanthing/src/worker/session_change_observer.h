#pragma once
#include <cstdint>
#include <memory>
#include <thread>
#include <atomic>

namespace lt
{

namespace worker
{

class SessionChangeObserver
{
public:
    static std::unique_ptr<SessionChangeObserver> create();
    void wait_for_change();
    void stop();

private:
    SessionChangeObserver() = default;
    void waiting_loop();

private:
    // std::thread thread_;
    std::atomic<bool> stoped_ { false };
    uint32_t startup_session_id_;
    std::wstring startup_desk_name_;
};

} // namespace worker

} // namespace lt