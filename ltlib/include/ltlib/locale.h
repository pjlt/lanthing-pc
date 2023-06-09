#pragma once

#include "ltlib.h"

#include <string>

namespace ltlib
{

// Convert a wide Unicode string to an UTF8 string
std::string LT_API wideCharToUtf8(const std::wstring& wstr);
// Convert an UTF8 string to a wide Unicode String
std::wstring LT_API utf8ToWideChar(const std::string& str);

} // namespace ltlib
