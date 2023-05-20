#pragma once
#include <string>

namespace ltlib
{

std::wstring utf8_to_utf16(const std::string& str);

std::string utf16_to_utf8(const std::wstring& str);

std::string random_str(size_t len);

} // namespace ltlib
