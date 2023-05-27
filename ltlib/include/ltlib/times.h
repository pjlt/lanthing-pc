#pragma once
#include <ltlib/ltlib.h>
#include <cstdint>

namespace ltlib
{

int64_t LT_API steady_now_us();
int64_t LT_API steady_now_ms();
int64_t LT_API utc_now_us();
int64_t LT_API utc_now_ms();

} // namespace ltlib