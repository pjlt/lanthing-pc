/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2023 Zhennan Tu <zhennan.tu@gmail.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once
#include <cstdint>
#include <string>

namespace ltlib {

int64_t steady_now_us();
int64_t steady_now_ms();
int64_t utc_now_us();
int64_t utc_now_ms();

class TimeDelta {
public:
    explicit TimeDelta(int64_t v)
        : delta_us_(v) {}

    int64_t value() const { return delta_us_; }

    std::string to_str() const;

private:
    int64_t delta_us_ = 0;
};

inline bool operator<(const TimeDelta& t1, const TimeDelta& t2) {
    return t1.value() < t2.value();
}

inline bool operator<=(const TimeDelta& t1, const TimeDelta& t2) {
    return t1.value() <= t2.value();
}

inline bool operator>(const TimeDelta& t1, const TimeDelta& t2) {
    return t1.value() > t2.value();
}

inline bool operator>=(const TimeDelta& t1, const TimeDelta& t2) {
    return t1.value() >= t2.value();
}

inline bool operator==(const TimeDelta& t1, const TimeDelta& t2) {
    return t1.value() == t2.value();
}

inline bool operator!=(const TimeDelta& t1, const TimeDelta& t2) {
    return !(t1.value() == t2.value());
}

/**
 *加减乘除
 */
template <typename T> inline TimeDelta operator+(const TimeDelta d1, const T d2) {
    // 不允许使用 1_ms + 1 这样的操作
    // 正确使用格式为: 1_ms + 1_ms （两边都是TimeDelta类型)
    static_assert(!std::is_integral<T>::value, "1_ms + 1 is not allowed");
    return TimeDelta(d1.value() + d2.value());
}

template <typename T> inline TimeDelta operator-(const TimeDelta d1, const T d2) {
    static_assert(!std::is_integral<T>::value, "1_ms - 1 is not allowed");
    return TimeDelta(d1.value() - d2.value());
}

template <typename T> inline TimeDelta operator*(const TimeDelta d1, const T d2) {
    // 不允许 3ms * 2us 等乱七八糟的操作
    // 只可以 3ms * 2 = 6ms
    static_assert(std::is_integral<T>::value || std::is_floating_point<T>::value,
                  "integral or float is required");
    // return TimeDelta(std::lround(d1.value() * d2 * 1.0));
    return TimeDelta(static_cast<int64_t>(d1.value() * d2 * 1.0 + 0.5));
}

template <typename T> inline TimeDelta operator*(const T d2, const TimeDelta d1) {
    return d1 * d2;
}

template <typename T> inline TimeDelta operator/(const TimeDelta d1, const T d2) {
    static_assert(std::is_integral<T>::value || std::is_floating_point<T>::value,
                  "integral or float is required");
    TimeDelta ret(0);
    // XXX:是否该替使用者考虑除数为0的问题
    // 除数为0，就让它奔溃好了
    // ret = TimeDelta(std::lround(d1.value() * 1.0 / d2 ));
    ret = TimeDelta(static_cast<int64_t>(d1.value() * 1.0 / d2 + 0.5));
    return ret;
}
// 允许使用:
//       1ms / 1us = 1000;
//       1ms / 3ms = 0.33333
inline double operator/(const TimeDelta d1, const TimeDelta d2) {
    return d1.value() * 1.0 / d2.value();
}

class Timestamp {
    static int64_t kMicroSecondsPerSecond;
    static int64_t kMicroSecondsPerDay;

public:
    enum class Type : uint8_t {
        kSinceEpoch = 1,
        kSincePowerup,
    };

    Timestamp(int64_t time_ = 0)
        : microseconds_(time_) {}

    inline int64_t microseconds() const { return microseconds_; }

    Timestamp& operator+=(const TimeDelta dt) {
        microseconds_ += dt.value();
        return *this;
    }

    // 20220114 21:01:04:123456
    std::string to_str(bool show_year = true, bool show_microseconds = true) const;

    // 20220114.2101
    std::string to_str2() const;

    static Timestamp now(Type since_power_up = Type::kSincePowerup);

private:
    int64_t microseconds_ = 0;
};

inline bool operator<(const Timestamp& a, const Timestamp& b) {
    return a.microseconds() < b.microseconds();
}

inline bool operator<=(const Timestamp& a, const Timestamp& b) {
    return a.microseconds() <= b.microseconds();
}

inline bool operator>(const Timestamp& a, const Timestamp& b) {
    return b < a;
}

inline bool operator>=(const Timestamp& a, const Timestamp& b) {
    return b <= a;
}

inline bool operator==(const Timestamp& lhs, const Timestamp& rhs) {
    return lhs.microseconds() == rhs.microseconds();
}
inline bool operator!=(const Timestamp& lhs, const Timestamp& rhs) {
    return !(lhs.microseconds() == rhs.microseconds());
}

inline TimeDelta operator-(const Timestamp& lhs, const Timestamp& rhs) {
    return TimeDelta(lhs.microseconds() - rhs.microseconds());
}

inline Timestamp operator-(const Timestamp& lhs, const TimeDelta& dt) {
    return Timestamp(lhs.microseconds() - dt.value());
}

inline Timestamp operator+(const Timestamp& at_time, const TimeDelta dt) {
    return at_time.microseconds() + dt.value();
}

namespace time {
// user-defined literals
TimeDelta operator"" _hour(unsigned long long h);
TimeDelta operator"" _min(unsigned long long m);
TimeDelta operator"" _sec(unsigned long long s);
TimeDelta operator"" _ms(unsigned long long ms);
TimeDelta operator"" _us(unsigned long long us);

} // namespace time

} // namespace ltlib