#include "ct_smoother.h"

namespace lt {

// 先不做任何平滑
void CTSmoother::push(Frame frame) {
    std::lock_guard<std::mutex> lock(buf_mtx_);
    frames_.clear();
    frames_.push_back(frame);
}

void CTSmoother::pop() {
    std::lock_guard<std::mutex> lock(buf_mtx_);
    if (frames_.empty()) {
        return;
    }
    frames_.pop_front();
}

size_t CTSmoother::size() const {
    std::lock_guard<std::mutex> lock(buf_mtx_);
    return frames_.size();
}

void CTSmoother::clear() {
    std::lock_guard<std::mutex> lock(buf_mtx_);
    frames_.clear();
}

int64_t CTSmoother::get(int64_t at_time) {
    (void)at_time;
    std::lock_guard<std::mutex> lock(buf_mtx_);
    if (frames_.empty()) {
        return -1;
    }
    return frames_.front().no;
}
} // namespace lt
