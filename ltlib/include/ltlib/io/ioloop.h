#pragma once
#include <ltlib/ltlib.h>
#include <functional>
#include <memory>

namespace ltlib
{

class IOLoopImpl;

class LT_API IOLoop
{
public:
    static std::unique_ptr<IOLoop> create();
    ~IOLoop() = default;
    IOLoop(const IOLoop&) = delete;
    IOLoop(IOLoop&&) = delete;
    IOLoop& operator=(const IOLoop&) = delete;
    IOLoop& operator=(IOLoop&&) = delete;
    void run(const std::function<void()>& i_am_alive);
    void stop();
    void post(const std::function<void()>& task);
    void post_delay(int64_t delay_ms, const std::function<void()>& task);
    bool is_current_thread() const;
    bool is_not_current_thread() const;
    void* context();

private:
    IOLoop() = default;

private:
    std::shared_ptr<IOLoopImpl> impl_;
};

} // namespace ltlib