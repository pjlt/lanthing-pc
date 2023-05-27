#pragma once
#include <ltlib/ltlib.h>
#include <atomic>

namespace ltlib
{

class LT_API SpinMutex
{
public:
    SpinMutex() = default;
    ~SpinMutex() = default;
    void lock()
    {
        bool expect_value = false;
        while (!flag_.compare_exchange_strong(expect_value, true,
            std::memory_order::memory_order_acquire,
            std::memory_order::memory_order_relaxed)) {
            expect_value = false;
        }
    }
    void unlock()
    {
        flag_.store(false);
    }

private:
    SpinMutex(SpinMutex&) = delete;
    SpinMutex(SpinMutex&&) = delete;
    SpinMutex operator=(const SpinMutex&) = delete;
    SpinMutex operator=(const SpinMutex&&) = delete;

    std::atomic<bool> flag_ { false };
};

} // namespace ltlib