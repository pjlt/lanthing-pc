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

#include <chrono>
#include <inttypes.h>
#include <ltlib/times.h>

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
#endif // __GNUC__

namespace ltlib {

int64_t steady_now_us() {
    int64_t now = std::chrono::duration_cast<std::chrono::microseconds>(
                      std::chrono::steady_clock::now().time_since_epoch())
                      .count();
    return now;
}

int64_t steady_now_ms() {
    int64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
                      std::chrono::steady_clock::now().time_since_epoch())
                      .count();
    return now;
}

int64_t utc_now_us() {
    int64_t now = std::chrono::duration_cast<std::chrono::microseconds>(
                      std::chrono::system_clock::now().time_since_epoch())
                      .count();
    return now;
}
int64_t utc_now_ms() {
    int64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
                      std::chrono::system_clock::now().time_since_epoch())
                      .count();
    return now;
}

std::string TimeDelta::to_str() const {
    std::string log;
    if (delta_us_ == std::numeric_limits<int64_t>::max()) {
        log = "+inf";
        return log;
    }
    if (delta_us_ == std::numeric_limits<int64_t>::min()) {
        log = "-inf";
        return log;
    }
    char buf[64] = {0};
    std::string unit;
    int64_t divisor = 1;
    if (delta_us_ < 1000) {
        unit = "us";
    }
    else {
        unit = "ms";
        divisor = 1000;
    }
    if (divisor > 1 && delta_us_ % divisor) {
        snprintf(buf, sizeof buf, "%.3f", delta_us_ * 1.0 / divisor);
    }
    else {
        snprintf(buf, sizeof buf, "%" PRId64, delta_us_ / divisor);
    }

    return std::string(buf) + unit;
}

int64_t Timestamp::kMicroSecondsPerSecond = 1000 * 1000;
int64_t Timestamp::kMicroSecondsPerDay = kMicroSecondsPerSecond * 24 * 60 * 60;

Timestamp Timestamp::now(Timestamp::Type t) {
    if (t == Type::kSincePowerup) {
        return std::chrono::duration_cast<std::chrono::microseconds>(
                   std::chrono::steady_clock::now().time_since_epoch())
            .count();
    }
    return Timestamp((static_cast<int64_t>(8) * 3600) * kMicroSecondsPerSecond +
                     std::chrono::duration_cast<std::chrono::microseconds>(
                         std::chrono::system_clock::now().time_since_epoch())
                         .count());
}

std::string Timestamp::to_str(bool show_year, bool show_microseconds) const {
    char buf[32] = {0};
    time_t seconds = static_cast<time_t>(microseconds_ / kMicroSecondsPerSecond);
    struct tm tm_time;
#if defined(WIN32) || defined(_WIN32)
    gmtime_s(&tm_time, &seconds);
#else
    gmtime_r(&seconds, &tm_time);
#endif

    if (show_microseconds) {
        int microseconds = static_cast<int>(microseconds_ % kMicroSecondsPerSecond);
        if (show_year)
            snprintf(buf, sizeof(buf), "%4d%02d%02d %02d:%02d:%02d.%06d", tm_time.tm_year + 1900,
                     tm_time.tm_mon + 1, tm_time.tm_mday, tm_time.tm_hour, tm_time.tm_min,
                     tm_time.tm_sec, microseconds);
        else
            snprintf(buf, sizeof(buf), "%02d%02d %02d:%02d:%02d.%06d", tm_time.tm_mon + 1,
                     tm_time.tm_mday, tm_time.tm_hour, tm_time.tm_min, tm_time.tm_sec,
                     microseconds);
    }
    else {
        if (show_year)
            snprintf(buf, sizeof(buf), "%4d%02d%02d %02d:%02d:%02d", tm_time.tm_year + 1900,
                     tm_time.tm_mon + 1, tm_time.tm_mday, tm_time.tm_hour, tm_time.tm_min,
                     tm_time.tm_sec);
        else
            snprintf(buf, sizeof(buf), "%02d%02d %02d:%02d:%02d", tm_time.tm_mon + 1,
                     tm_time.tm_mday, tm_time.tm_hour, tm_time.tm_min, tm_time.tm_sec);
    }
    return buf;
}

std::string Timestamp::to_str2() const {
    char buf[32] = {0};
    time_t seconds = static_cast<time_t>(microseconds_ / kMicroSecondsPerSecond);
    struct tm tm_time;
#if defined(WIN32) || defined(_WIN32)
    gmtime_s(&tm_time, &seconds);
#else
    gmtime_r(&seconds, &tm_time);
#endif
    snprintf(buf, sizeof(buf), "%4d%02d%02d.%02d%02d", tm_time.tm_year + 1900, tm_time.tm_mon + 1,
             tm_time.tm_mday, tm_time.tm_hour, tm_time.tm_min);
    return buf;
}

namespace time {
// TODO: day and month?
TimeDelta operator"" _hour(unsigned long long h) {
    return TimeDelta(static_cast<int64_t>(h) * 60 * 60 * 1000 * 1000);
}
TimeDelta operator"" _min(unsigned long long m) {
    return TimeDelta(static_cast<int64_t>(m) * 60 * 1000 * 1000);
}
TimeDelta operator"" _sec(unsigned long long s) {
    return TimeDelta(static_cast<int64_t>(s) * 1000 * 1000);
}
TimeDelta operator"" _ms(unsigned long long ms) {
    return TimeDelta(static_cast<int64_t>(ms) * 1000);
}
TimeDelta operator"" _us(unsigned long long us) {
    return TimeDelta(static_cast<int64_t>(us) * 1);
}

} // namespace time

} // namespace ltlib

#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif // __GNUC__
