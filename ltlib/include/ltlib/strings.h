#pragma once
#include <ltlib/ltlib.h>
#include <string>

namespace ltlib
{

std::wstring LT_API utf8_to_utf16(const std::string& str);

std::string LT_API utf16_to_utf8(const std::wstring& str);

std::string LT_API random_str(size_t len);

} // namespace ltlib
