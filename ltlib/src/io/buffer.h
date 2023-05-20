#pragma once
#include <cstdint>

namespace ltlib
{

struct Buffer
{
    explicit Buffer(uint32_t l)
        : Buffer { new char[l], l }
    {
    }
#ifdef LT_WINDOWS
    Buffer(char* b, uint32_t l)
        : len { l }
        , base { b }
    {
    }
    uint32_t len;
    char* base;
#else
    Buffer(char* b, uint32_t l)
        : base { b }
        , len { l }
    {
    }
    char* base;
    uint32_t len;

#endif
};

} // namespace ltlib