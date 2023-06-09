#include "ltlib/locale.h"

#if defined(LT_WINDOWS)
#include <Windows.h>
#endif

namespace ltlib
{

std::string wideCharToUtf8(const std::wstring& wstr)
{
    if (wstr.empty())
        return std::string();
#if defined(MLIB_WIN)
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
    std::string strTo(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
    return strTo;
#endif
    return "not implement";
}

std::wstring utf8ToWideChar(const std::string& str)
{
    if (str.empty())
        return std::wstring();
#if defined(MLIB_WIN)
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
    std::wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
    return wstrTo;
#endif
    return std::wstring(L"not implement");
}
} // namespace ltlib
