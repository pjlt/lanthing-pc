#include "ct_smoother.h"

namespace lt
{

void CTSmoother::push(Frame frame) {
    frames_.push_back(frame);
}

void CTSmoother::pop() {
    if (frames_.empty()) {
        return;
    }
    frames_.pop_front();
}

void CTSmoother::clear() {
    frames_.clear();
}

int64_t CTSmoother::get(int64_t at_time) {
    if (frames_.empty()) {
        return -1;
    }
    return frames_.front().no;
}
} // namespace lt