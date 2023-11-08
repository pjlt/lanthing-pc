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

#if defined(LT_WINDOWS)
#include <Windows.h>
#else
#endif // LT_WINDOWS
#include <ltlib/event.h>
#include <ltlib/strings.h>
#include <assert.h>

namespace ltlib
{

#if defined(LT_WINDOWS)

Event::Event() noexcept
{
    handle_ = ::CreateEventW(NULL, FALSE, FALSE, NULL);
    assert(handle_ != NULL);
    is_owner_ = true;
}

Event::Event(const std::string& name) noexcept
    : name_ { name }
{
    assert(!name_.empty());
    std::wstring wname = utf8To16(name_);
    handle_ = ::CreateEventW(NULL, FALSE, FALSE, wname.c_str());
    if (::GetLastError() == 0) {
        is_owner_ = true;
    }
    assert(handle_ != NULL);
}

Event::Event(Event&& other) noexcept
    : name_(other.name_)
    , handle_(other.handle_)
    , is_owner_(other.is_owner_)
{
    other.name_.clear();
    other.handle_ = NULL;
    other.is_owner_ = false;
}

Event& Event::operator=(Event&& other) noexcept
{
    close();
    name_ = other.name_;
    handle_ = other.handle_;
    is_owner_ = other.is_owner_;
    other.name_.clear();
    other.handle_ = NULL;
    other.is_owner_ = false;
    return *this;
}

Event::~Event()
{
    close();
}

bool Event::notify()
{
    return ::SetEvent(handle_);
}

Event::WaitResult Event::wait()
{
    return waitFor(INFINITE);
}

Event::WaitResult Event::waitFor(uint32_t ms)
{
    DWORD ret = ::WaitForSingleObject(handle_, ms);
    if (ret == WAIT_OBJECT_0) {
        return WaitResult::Success;
    } else if (ret == WAIT_TIMEOUT) {
        return WaitResult::Timeout;
    } else {
        return WaitResult::Failed;
    }
}

void* Event::getHandle() const
{
    return handle_;
}

void Event::close()
{
    if (handle_ != NULL) {
        ::CloseHandle(handle_);
        handle_ = NULL;
    }
}

#else // LT_WINDOWS

// 在跨进程使用pthread的mutex和condition variable，需要使用共享内存，有点麻烦

Event::Event() noexcept
{
}

Event::Event(const std::string& name) noexcept
{
    (void)name;
}
Event::Event(Event&& other) noexcept
{
    (void)other;
}

Event& Event::operator=(Event&& other) noexcept
{
    (void)other;
    return *this;
}

Event::~Event()
{
}

bool Event::notify()
{
    return false;
}

Event::WaitResult Event::wait()
{
    return WaitResult::Failed;
}

Event::WaitResult Event::waitFor(uint32_t ms)
{
    (void)ms;
    return WaitResult::Failed;
}

void* Event::getHandle() const
{
    return nullptr;
}

#endif

} // namespace ltlib
