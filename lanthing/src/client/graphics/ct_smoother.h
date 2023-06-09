#pragma once

#include <cstdint>
#include <deque>
#include <mutex>

namespace lt {

// 平滑算法
class CTSmoother {
public:
    struct Frame {
        int64_t no;

        int64_t at_time = 0;
        int64_t capture_time = 0;
    };

public:
    void push(Frame frame);

    void pop();

    int64_t get(int64_t at_time);

    void clear();

    size_t size() const;

private:
    mutable std::mutex buf_mtx_;
    std::deque<Frame> frames_;
};
} // namespace lt
