#pragma once
#include <ltlib/ltlib.h>
#include <cstdint>
#include <memory>
#include <string>
#include <functional>
#include <future>
#include <thread>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <map>
#include <queue>

#include <ltlib/times.h>

namespace ltlib
{

class LT_API ThreadWatcher
{
public:
    static constexpr int64_t kMaxBlockTimeMS = 5'000;

public:
    static ThreadWatcher* instance();
    ~ThreadWatcher();
    void add(const std::string& name, std::thread::id thread_id);
    void remove(const std::string& name);
    void report_alive(const std::string& name);
    void register_terminate_callback(const std::function<void(const std::string&)>& callback);
    void enable_crash_on_timeout();
    void disable_crash_on_timeout();

private:
    ThreadWatcher();
    ThreadWatcher(const ThreadWatcher&) = delete;
    ThreadWatcher(ThreadWatcher&&) = delete;
    ThreadWatcher& operator=(const ThreadWatcher&) = delete;
    ThreadWatcher& operator=(const ThreadWatcher&&) = delete;
    void check_loop();

private:
    struct ThreadInfo
    {
        std::string name;
        std::thread::id thread_id;
        int64_t last_active_time;
    };
    std::thread thread_;
    std::mutex mutex_;
    std::condition_variable cv_;
    bool stoped_ = false;
    std::map<std::string, ThreadInfo> threads_;
    std::function<void(const std::string&)> terminate_callback_;
    std::atomic<bool> enable_crash_ { true };
};

class LT_API BlockingThread
{
public:
    using EntryFunction = std::function<void(std::function<void()>/*i_am_alive*/, void*/*user_data*/)>;

public:
    static std::unique_ptr<BlockingThread> create(const std::string& prefix, const EntryFunction& user_func, void* user_data);
    bool is_current_thread() const;
    ~BlockingThread();

private:
    BlockingThread(const std::string& prefix, const EntryFunction& func, void* user_data);
    BlockingThread(const BlockingThread&) = delete;
    BlockingThread& operator=(const BlockingThread&) = delete;
    BlockingThread(BlockingThread&&) = delete;
    BlockingThread& operator=(BlockingThread&&) = delete;
    void start();
    void main_loop(std::promise<void>& promise);
    void register_to_thread_watcher();
    void unregister_from_thread_watcher();
    void i_am_alive();
    void set_thread_name();

private:
    std::thread thread_;
    std::string name_;
    const EntryFunction user_func_;
    void* user_data_;
    int64_t last_report_time_;
};

enum class LT_API Priority : uint32_t
{
    Low,
    Medium,
    High,
};
inline bool operator<(const Priority& left, const Priority& right)
{
    return static_cast<std::underlying_type_t<Priority>>(left) < static_cast<std::underlying_type_t<Priority>>(right);
}



class LT_API TaskThread
{
public:
    using Task = std::function<void()>;
    using TimerID = int64_t;

public:
    static std::unique_ptr<TaskThread> create(const std::string& prefix);
    ~TaskThread();
    void post(const Task& task);
    TimerID post_delay(TimeDelta delta_time, const Task& task);
    void cancel(TimerID timer);
    bool is_current_thread();
    void wake();
    bool is_running();

    template <typename ReturnT, typename = std::enable_if<!std::is_void<ReturnT>::value>::type>
    ReturnT invoke(std::function<ReturnT(void)> func)
    {
        ReturnT ret;
        invokeInternal([func, &ret]() { ret = func(); });
        return ret;
    }

    template <typename ReturnT, typename = std::enable_if<std::is_void<ReturnT>::value>::type>
    void invoke(std::function<ReturnT(void)> task)
    {
        invokeInternal(task);
    }

private:
    TaskThread(const std::string& prfix);
    TaskThread(TaskThread&&) = delete;
    TaskThread& operator=(TaskThread&&) = delete;
    TaskThread(TaskThread&) = delete;
    TaskThread& operator=(TaskThread&) = delete;
    void start();
    void main_loop(std::promise<void>& promise);
    void wake_up();
    void register_to_thread_watcher();
    void unregister_from_thread_watcher();
    void i_am_alive();
    void set_thread_name();
    void invokeInternal(const Task& task);
    inline std::deque<Task> get_pending_tasks();
    inline std::tuple<std::vector<Task>, TimeDelta> get_timeup_delay_tasks();

private:
    std::string name_;
    std::deque<Task> tasks_;
    std::map<Timestamp, Task> delay_tasks_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::atomic<bool> wakeup_ { true };
    std::thread thread_;
    bool started_ = false;
    bool stoped_ = false;
    int64_t last_report_time_;
};

} // namespace ltlib