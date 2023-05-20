#include <ltlib/strings.h>
#include <utf8.h>

namespace ltlib
{

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