#pragma once
#include <ltlib/ltlib.h>
#include <string>

namespace ltlib
{

class LT_API Event
{
public:
    enum class WaitResult
    {
        Success,
        Timeout,
        Failed,
    };

public:
    // create event without a name
    Event() noexcept;
    // create named event
    explicit Event(const std::string& name) noexcept;
    Event(Event&& other) noexcept;
    Event& operator=(Event&& other) noexcept;
    Event(Event&) = delete;
    Event& operator=(Event&) = delete;
    ~Event();

    bool notify();
    WaitResult wait();
    WaitResult waitFor(uint32_t ms);
    inline bool isOwner() const { return is_owner_; }
    void* getHandle() const;

private:
    void close();

private:
    std::string name_;
    void* handle_;
    bool is_owner_ = false;
};

} // namespace ltlib
