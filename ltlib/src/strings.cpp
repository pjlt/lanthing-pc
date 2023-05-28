#include <ltlib/strings.h>
#include <type_traits>
#include <utf8.h>

namespace
{

// These must be (unsigned) long long, to match the signature of strto(u)ll.
using unsigned_type = unsigned long long; // NOLINT(runtime/int)
using signed_type = long long; // NOLINT(runtime/int)

std::optional<signed_type> ParseSigned(const std::string& str, int base)
{
    if (str.empty()) {
        return std::nullopt;
    }
    if (isdigit(str[0]) || str[0] == '-') {
        std::string str_str = std::string(str);
        char* end = nullptr;
        errno = 0;
        const signed_type value = std::strtoll(str_str.c_str(), &end, base);
        // Check for errors and also make sure that there were no embedded nuls in
        // the input string.
        if (end == str_str.c_str() + str_str.size() && errno == 0) {
            return value;
        }
    }
    return std::nullopt;
}

std::optional<unsigned_type> ParseUnsigned(const std::string& str, int base)
{
    if (str.empty()) {
        return std::nullopt;
    }

    if (isdigit(str[0]) || str[0] == '-') {
        std::string str_str = std::string(str);
        // Explicitly discard negative values. std::strtoull parsing causes unsigned
        // wraparound. We cannot just reject values that start with -, though, since
        // -0 is perfectly fine, as is -0000000000000000000000000000000.
        const bool is_negative = str[0] == '-';
        char* end = nullptr;
        errno = 0;
        const unsigned_type value = std::strtoull(str_str.c_str(), &end, base);
        // Check for errors and also make sure that there were no embedded nuls in
        // the input string.
        if (end == str_str.c_str() + str_str.size() && errno == 0 && (value == 0 || !is_negative)) {
            return value;
        }
    }
    return std::nullopt;
}

template <typename T>
T StrToT(const char* str, char** str_end);

template <>
inline float StrToT(const char* str, char** str_end)
{
    return std::strtof(str, str_end);
}

template <>
inline double StrToT(const char* str, char** str_end)
{
    return std::strtod(str, str_end);
}

template <>
inline long double StrToT(const char* str, char** str_end)
{
    return std::strtold(str, str_end);
}

template <typename T>
std::optional<T> ParseFloatingPoint(const std::string& str)
{
    if (str.empty())
        return std::nullopt;

    if (str[0] == '\0')
        return std::nullopt;
    std::string str_str = std::string(str);
    char* end = nullptr;
    errno = 0;
    const T value = StrToT<T>(str_str.c_str(), &end);
    if (end == str_str.c_str() + str_str.size() && errno == 0) {
        return value;
    }
    return std::nullopt;
}

template <typename T>
typename std::enable_if<std::is_integral<T>::value && std::is_signed<T>::value,
    std::optional<T>>::type
StringToNumber(const std::string& str, int base = 10)
{
    static_assert(std::numeric_limits<T>::max() <= std::numeric_limits<signed_type>::max() && std::numeric_limits<T>::lowest() >= std::numeric_limits<signed_type>::lowest(),
        "StringToNumber only supports signed integers as large as long long int");
    std::optional<signed_type> value = ParseSigned(str, base);
    if (value && *value >= std::numeric_limits<T>::lowest() && *value <= std::numeric_limits<T>::max()) {
        return static_cast<T>(*value);
    }
    return std::nullopt;
}

template <typename T>
typename std::enable_if<std::is_integral<T>::value && std::is_unsigned<T>::value,
    std::optional<T>>::type
StringToNumber(const std::string& str, int base = 10)
{
    static_assert(std::numeric_limits<T>::max() <= std::numeric_limits<unsigned_type>::max(),
        "StringToNumber only supports unsigned integers as large as "
        "unsigned long long int");
    std::optional<unsigned_type> value = ParseUnsigned(str, base);
    if (value && *value <= std::numeric_limits<T>::max()) {
        return static_cast<T>(*value);
    }
    return std::nullopt;
}

template <typename T>
typename std::enable_if<std::is_floating_point<T>::value, std::optional<T>>::type
StringToNumber(const std::string& str, int base = 10)
{
    static_assert(std::numeric_limits<T>::max() <= std::numeric_limits<long double>::max(),
        "StringToNumber only supports floating-point numbers as large "
        "as long double");
    return ParseFloatingPoint<T>(str);
}

} // ÄäÃû¿Õ¼ä

namespace ltlib
{

bool String::getValue(const std::string& s, int64_t* t)
{
    auto num = StringToNumber<int64_t>(s, 10);
    if (!num) {
        return false;
    }

    *t = *num;
    return true;
}

bool String::getValue(const std::string& s, uint64_t* t)
{
    auto num = StringToNumber<uint64_t>(s, 10);
    if (!num) {
        return false;
    }

    *t = *num;
    return true;
}

bool String::getValue(const std::string& s, float* t)
{
    auto num = StringToNumber<float>(s, 10);
    if (!num) {
        return false;
    }

    *t = *num;
    return true;
}

bool String::getValue(const std::string& s, double* t)
{
    auto num = StringToNumber<double>(s, 10);
    if (!num) {
        return false;
    }

    *t = *num;
    return true;
}

bool String::getValue(const std::string& s, int32_t* t)
{
    int64_t value_64 = 0;
    if (!getValue(s, &value_64)) {
        return false;
    }
    *t = static_cast<int32_t>(value_64);
    return value_64 <= std::numeric_limits<int32_t>::max();
}

bool String::getValue(const std::string& s, uint32_t* t)
{
    uint64_t value_64 = 0;
    if (!getValue(s, &value_64)) {
        return false;
    }
    *t = static_cast<uint32_t>(value_64);
    return value_64 <= std::numeric_limits<uint32_t>::max();
}

bool String::getValue(const std::string& s, int16_t* t)
{
    int64_t value_64 = 0;
    if (!getValue(s, &value_64)) {
        return false;
    }
    *t = static_cast<int16_t>(value_64);
    return value_64 <= std::numeric_limits<int16_t>::max();
}

bool String::getValue(const std::string& s, uint16_t* t)
{
    uint64_t value_64 = 0;
    if (!getValue(s, &value_64)) {
        return false;
    }
    *t = static_cast<uint16_t>(value_64);
    return value_64 <= std::numeric_limits<uint16_t>::max();
}

std::wstring utf8_to_utf16(const std::string& str)
{
    if (str.empty()) {
        return {};
    }
    std::wstring result;
    utf8::utf8to16(str.begin(), str.end(), std::back_inserter(result));
    return result;
}

std::string utf16_to_utf8(const std::wstring& str)
{
    if (str.empty()) {
        return {};
    }
    std::string result;
    utf8::utf16to8(str.begin(), str.end(), std::back_inserter(result));
    return result;
}

std::string random_str(size_t len)
{
    static const char alphanum[] = "0123456789"
                                   "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                   "abcdefghijklmnopqrstuvwxyz";
    std::string tmp_s;
    tmp_s.reserve(len);

    for (int i = 0; i < len; ++i) {
        tmp_s += alphanum[rand() % (sizeof(alphanum) - 1)];
    }

    return tmp_s;
}

} // namespace ltlib