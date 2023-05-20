#pragma once
#include <cstdint>

namespace ltlib
{

int64_t steady_now_us();
int64_t steady_now_ms();
int64_t utc_now_us();
int64_t utc_now_ms();

} // namespace ltlib