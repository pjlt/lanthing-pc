#include <ltlib/reconnect_interval.h>

namespace ltlib
{

void ReconnectInterval::reset()
{
    index_ = 0;
}

int64_t ReconnectInterval::next()
{
    auto interval = kIntervalsMS[index_];
    index_ = std::min(index_ + 1, kIntervalsMS.size() - 1);
    return interval;
}

} // namespace ltlib