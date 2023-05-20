#include <ltlib/times.h>
#include <chrono>

namespace ltlib
{

int64_t steady_now_us()
{
    int64_t now = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch())
                      .count();
    return now;
}

int64_t steady_now_ms()
{
    int64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch())
                      .count();
    return now;
}

int64_t utc_now_us()
{
    int now = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch())
                  .count();
    return now;
}
int64_t utc_now_ms()
{
    int now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch())
                  .count();
    return now;
}

} // namespace ltlib