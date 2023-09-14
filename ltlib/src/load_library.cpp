/*
 * BSD 3-Clause License
 * 
 * Copyright (c) 2023 Zhennan Tu <zhennan.tu@gmail.com>
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *  
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

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
    std::wstring wpath = utf8To16(path);
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

void* DynamicLibrary::getFunc(const std::string& name)
{
    if (name.empty() || handle_ == nullptr) {
        return nullptr;
    }
    auto fptr = GetProcAddress(reinterpret_cast<HMODULE>(handle_), name.c_str());
    return fptr;
}

#else // LT_WINDOWS
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

void* DynamicLibrary::getFunc(const std::string& name)
{
    if (name.empty() || handle_ == nullptr) {
        return nullptr;
    }
    auto fptr = dlsym(handle_, name.c_str());
    return fptr;
}
#endif // LT_WINDOWS

} // namespace ltlib