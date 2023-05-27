#ifdef LT_WINDOWS
#include <Windows.h>
#else // LT_WINDOWS
#include <dlfcn.h>
#endif // LT_WINDOWS
#include <ltlib/load_library.h>
#include <ltlib/strings.h>

namespace ltlib
{

#ifdef LT_WINDOWS
std::unique_ptr<DynamicLibrary> DynamicLibrary::load(const std::string& path)
{
    if (path.empty()) {
        return nullptr;
    }
    std::wstring wpath = utf8_to_utf16(path);
    HMODULE lib = LoadLibraryW(wpath.c_str());
    if (lib == nullptr) {
        return nullptr;
    }
    std::unique_ptr<DynamicLibrary> dlib { new DynamicLibrary };
    dlib->handle_ = lib;
    return dlib;
}

DynamicLibrary::DynamicLibrary() = default;

DynamicLibrary::~DynamicLibrary()
{
    if (handle_) {
        FreeLibrary(reinterpret_cast<HMODULE>(handle_));
        handle_ = nullptr;
    }
}

void* DynamicLibrary::get_func(const std::string& name)
{
    if (name.empty() || handle_ == nullptr) {
        return nullptr;
    }
    auto fptr = GetProcAddress(reinterpret_cast<HMODULE>(handle_), name.c_str());
    return fptr;
}

#else //LT_WINDOWS
std::unique_ptr<DynamicLibrary> DynamicLibrary::load(const std::string& path)
{
    if (path.empty()) {
        return nullptr;
    }
    auto lib = dlopen(path.c_str(), RTLD_LAZY);
    if (lib == nullptr) {
        return nullptr;
    }
    auto dlib = std::make_unique<DynamicLibrary>();
    dlib->handle_ = lib;
    return dlib;
}

DynamicLibrary::~DynamicLibrary()
{
    if (handle_) {
        dlclose(handle_);
        handle_ = nullptr;
    }
}

void* DynamicLibrary::get_func(const std::string& name)
{
    if (name.empty() || handle_ == nullptr) {
        return nullptr;
    }
    auto fptr = dlsym(handle_, name.c_str());
    return fptr;
}
#endif //LT_WINDOWS

} // namespace ltlib