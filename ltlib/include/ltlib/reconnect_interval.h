#pragma once
#include <cstdint>
#include <array>

namespace ltlib
{

class ReconnectInterval
{
public:
    ReconnectInterval() = default;
    void reset();
    int64_t next();

private:
    static constexpr std::array<int64_t, 8>kIntervalsMS { 100, 500, 1'000, 2'000, 5'000, 10'000, 30'000, 60'000 };
    size_t index_ = 0;
};

} // namespace ltlib