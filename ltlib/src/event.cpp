#if defined(LT_WINDOWS)
#include <Windows.h>
#else
#endif
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
    std::wstring wname = utf8_to_utf16(name_);
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
    return wait_for(INFINITE);
}

Event::WaitResult Event::wait_for(uint32_t ms)
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

void* Event::get_handle() const
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

#else

// 在跨进程使用pthread的mutex和condition variable，需要使用共享内存，有点麻烦

Event::Event() noexcept
{
}

Event::Event(const std::string& name) noexcept
{
}
Event::Event(Event&& other) noexcept
{
}

Event::Event& operator=(Event&& other) noexcept
{
}

Event::~Event()
{
}

bool Event::notify()
{
}

bool Event::wait()
{
}

bool Event::wait_for(uint32_t ms)
{
}

void* Event::get_handle() const
{
}

#endif

} // namespace ltlib
