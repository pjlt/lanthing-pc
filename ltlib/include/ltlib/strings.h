#pragma once
#include <ltlib/ltlib.h>
#include <string>
#include <optional>

namespace ltlib
{

std::wstring LT_API utf8To16(const std::string& str);

std::string LT_API utf16To8(const std::wstring& str);

std::string LT_API randomStr(size_t len);

// 返回std::optional是最好的方式，这里倒退回老派C接口了
class LT_API String
{
public:
    static bool getValue(const std::string& s, int16_t* t);
    static bool getValue(const std::string& s, uint16_t* t);

    static bool getValue(const std::string& s, int32_t* t);
    static bool getValue(const std::string& s, uint32_t* t);

    static bool getValue(const std::string& s, int64_t* t);
    static bool getValue(const std::string& s, uint64_t* t);

    static bool getValue(const std::string& s, float* t);
    static bool getValue(const std::string& s, double* t);
};

} // namespace ltlib
