#pragma once
#include <ltlib/ltlib.h>
#include <cstdint>
#include <string>

namespace ltlib
{

int64_t LT_API steady_now_us();
int64_t LT_API steady_now_ms();
int64_t LT_API utc_now_us();
int64_t LT_API utc_now_ms();

class LT_API TimeDelta
{
public:
    explicit TimeDelta(int64_t v)
        : delta_us_(v)
    {
    }

    int64_t value() const { return delta_us_; }

    std::string to_str() const;

private:
    int64_t delta_us_ = 0;
};

inline bool operator<(const TimeDelta& t1, const TimeDelta& t2)
{
    return t1.value() < t2.value();
}

inline bool operator<=(const TimeDelta& t1, const TimeDelta& t2)
{
    return t1.value() <= t2.value();
}

inline bool operator>(const TimeDelta& t1, const TimeDelta& t2)
{
    return t1.value() > t2.value();
}

inline bool operator>=(const TimeDelta& t1, const TimeDelta& t2)
{
    return t1.value() >= t2.value();
}

inline bool operator==(const TimeDelta& t1, const TimeDelta& t2)
{
    return t1.value() == t2.value();
}

inline bool operator!=(const TimeDelta& t1, const TimeDelta& t2)
{
    return !(t1.value() == t2.value());
}

/**
 *加减乘除
 */
template <typename T>
inline TimeDelta operator+(const TimeDelta d1, const T d2)
{
    // 不允许使用 1_ms + 1 这样的操作
    // 正确使用格式为: 1_ms + 1_ms （两边都是TimeDelta类型)
    static_assert(!std::is_integral<T>::value, "1_ms + 1 is not allowed");
    return TimeDelta(d1.value() + d2.value());
}

template <typename T>
inline TimeDelta operator-(const TimeDelta d1, const T d2)
{
    static_assert(!std::is_integral<T>::value, "1_ms - 1 is not allowed");
    return TimeDelta(d1.value() - d2.value());
}

template <typename T>
inline TimeDelta operator*(const TimeDelta d1, const T d2)
{
    // 不允许 3ms * 2us 等乱七八糟的操作
    // 只可以 3ms * 2 = 6ms
    static_assert(std::is_integral<T>::value || std::is_floating_point<T>::value,
        "integral or float is required");
    // return TimeDelta(std::lround(d1.value() * d2 * 1.0));
    return TimeDelta(static_cast<int64_t>(d1.value() * d2 * 1.0 + 0.5));
}

template <typename T>
inline TimeDelta operator*(const T d2, const TimeDelta d1)
{
    return d1 * d2;
}

template <typename T>
inline TimeDelta operator/(const TimeDelta d1, const T d2)
{
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
inline double operator/(const TimeDelta d1, const TimeDelta d2)
{
    return d1.value() * 1.0 / d2.value();
}


class LT_API Timestamp
{
    static int64_t kMicroSecondsPerSecond;
    static int64_t kMicroSecondsPerDay;

public:
    enum class Type : uint8_t
    {
        kSinceEpoch = 1,
        kSincePowerup,
    };

    Timestamp(int64_t time_ = 0)
        : microseconds_(time_)
    {
    }

    inline int64_t microseconds() const { return microseconds_; }

    Timestamp& operator+=(const TimeDelta dt)
    {
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

inline bool operator<(const Timestamp& a, const Timestamp& b)
{
    return a.microseconds() < b.microseconds();
}

inline bool operator<=(const Timestamp& a, const Timestamp& b)
{
    return a.microseconds() <= b.microseconds();
}

inline bool operator>(const Timestamp& a, const Timestamp& b)
{
    return b < a;
}

inline bool operator>=(const Timestamp& a, const Timestamp& b)
{
    return b <= a;
}

inline bool operator==(const Timestamp& lhs, const Timestamp& rhs)
{
    return lhs.microseconds() == rhs.microseconds();
}
inline bool operator!=(const Timestamp& lhs, const Timestamp& rhs)
{
    return !(lhs.microseconds() == rhs.microseconds());
}

inline TimeDelta operator-(const Timestamp& lhs, const Timestamp& rhs)
{
    return TimeDelta(lhs.microseconds() - rhs.microseconds());
}

inline Timestamp operator-(const Timestamp& lhs, const TimeDelta& dt)
{
    return Timestamp(lhs.microseconds() - dt.value());
}

inline Timestamp operator+(const Timestamp& at_time, const TimeDelta dt)
{
    return at_time.microseconds() + dt.value();
}


namespace time
{
// user-defined literals
TimeDelta LT_API operator"" _hour(unsigned long long h);
TimeDelta LT_API operator"" _min(unsigned long long m);
TimeDelta LT_API operator"" _sec(unsigned long long s);
TimeDelta LT_API operator"" _ms(unsigned long long ms);
TimeDelta LT_API operator"" _us(unsigned long long us);

} // namespace time

} // namespace ltlib