#pragma once
#include <ltlib/ltlib.h>
#include <memory>
#include <string>

namespace ltlib
{

class LT_API DynamicLibrary
{
public:
    static std::unique_ptr<DynamicLibrary> load(const std::string& path);
    ~DynamicLibrary();
    void* get_func(const std::string& name);

private:
    DynamicLibrary();

private:
    void* handle_;
};


} // namespace ltlib