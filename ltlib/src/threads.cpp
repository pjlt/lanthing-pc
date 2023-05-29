#if defined(LT_WINDOWS)
#include <Windows.h>
#endif

#include <sstream>
#include <ltlib/times.h>
#include <ltlib/threads.h>

namespace
{

using SetThreadDescriptionFunc = HRESULT(WINAPI*)(HANDLE hThread, PCWSTR lpThreadDescription);

void set_current_thread_name(const char* name)
{
#if defined(LT_WINDOWS)
    static auto set_thread_description_func = reinterpret_cast<SetThreadDescriptionFunc>(::GetProcAddress(
        ::GetModuleHandleA("Kernel32.dll"), "SetThreadDescription"));
    if (set_thread_description_func) {
        // Convert from ASCII to UTF-16.
        wchar_t wide_thread_name[64];
        for (size_t i = 0; i < (sizeof(wide_thread_name) / sizeof(wide_thread_name[0])) - 1; ++i) {
            wide_thread_name[i] = name[i];
            if (wide_thread_name[i] == L'\0')
                break;
        }
        // Guarantee null-termination.
        wide_thread_name[sizeof(wide_thread_name) / sizeof(wide_thread_name[0]) - 1] = L'\0';
        set_thread_description_func(::GetCurrentThread(), wide_thread_name);
    }

    // For details see:
    // https://docs.microsoft.com/en-us/visualstudio/debugger/how-to-set-a-thread-name-in-native-code
#pragma pack(push, 8)
    struct
    {
        DWORD dwType;
        LPCSTR szName;
        DWORD dwThreadID;
        DWORD dwFlags;
    } threadname_info = { 0x1000, name, static_cast<DWORD>(-1), 0 };
#pragma pack(pop)

#pragma warning(push)
#pragma warning(disable : 6320 6322)
    __try {
        ::RaiseException(0x406D1388, 0, sizeof(threadname_info) / sizeof(ULONG_PTR),
            reinterpret_cast<ULONG_PTR*>(&threadname_info));
    } __except (EXCEPTION_EXECUTE_HANDLER) { // NOLINT
    }
#pragma warning(pop)
#elif defined(LT_LINUX) || defined(LT_ANDROID)
    prctl(PR_SET_NAME, reinterpret_cast<unsigned long>(name));
#elif defined(LT_MAC) || defined(LT_IOS)
    pthread_setname_np(name);
#endif
}

void crash_me()
{
    int* a = 0;
    *a = 123;
}

} // 匿名空间

namespace ltlib
{

using namespace time;

ThreadWatcher* ThreadWatcher::instance()
{
    static ThreadWatcher* const thread_watcher = new ThreadWatcher;
    return thread_watcher;
}

ThreadWatcher::ThreadWatcher()
    : thread_ { std::bind(&ThreadWatcher::check_loop, this) }
{
}

ThreadWatcher::~ThreadWatcher()
{
    {
        std::lock_guard lock { mutex_ };
        stoped_ = true;
    }
    cv_.notify_one();
    thread_.join();
}

void ThreadWatcher::add(const std::string& name, std::thread::id thread_id)
{
    // 由调用者保证name的唯一性
    ThreadInfo info { name, thread_id, ltlib::steady_now_ms() };
    std::lock_guard lock { mutex_ };
    threads_[name] = info;
}

void ThreadWatcher::remove(const std::string& name)
{
    std::lock_guard lock { mutex_ };
    threads_.erase(name);
}

void ThreadWatcher::register_terminate_callback(const std::function<void(const std::string&)>& callback)
{
    std::lock_guard lock { mutex_ };
    terminate_callback_ = callback;
}

void ThreadWatcher::enable_crash_on_timeout()
{
    enable_crash_ = true;
}

void ThreadWatcher::disable_crash_on_timeout()
{
    enable_crash_ = false;
}

void ThreadWatcher::check_loop()
{
    set_current_thread_name("dead_thread_checker");
    while (true) {
        std::unique_lock lock { mutex_ };
        cv_.wait_for(lock, std::chrono::milliseconds { 700 }, [this]() { return stoped_; });
        if (stoped_) {
            return;
        }
        int64_t now = steady_now_ms();
        for (auto& th : threads_) {
            if (now - th.second.last_active_time > kMaxBlockTimeMS) {
                if (terminate_callback_) {
                    std::stringstream ss;
                    ss << "Thread(" << th.second.name << ':' << th.second.thread_id << ") inactive for " << now - th.second.last_active_time << "ms";
                    terminate_callback_(ss.str());
                }
                if (enable_crash_) {
                    //std::terminate();
                    crash_me();
                }
            }
        }
    }
}

void ThreadWatcher::report_alive(const std::string& name)
{
    std::lock_guard lock { mutex_ };
    auto iter = threads_.find(name);
    if (iter != threads_.end()) {
        iter->second.last_active_time = ltlib::steady_now_ms();
    }
}

BlockingThread::BlockingThread(const std::string& prefix, const EntryFunction& func, void* user_data)
    : user_func_ { func }
    , user_data_ {user_data}
    , last_report_time_ { ltlib::steady_now_ms() }
{
    std::stringstream ss;
    ss << prefix << '-' << std::hex << (int64_t)this;
    name_ = ss.str();
}

BlockingThread::~BlockingThread()
{
    if (thread_.joinable()) {
        thread_.join();
    }
}

std::unique_ptr<BlockingThread> BlockingThread::create(const std::string& prefix, const EntryFunction& func, void* user_data)
{
    if (prefix.empty() || func == nullptr) {
        return nullptr;
    }
    std::unique_ptr<BlockingThread> bthread { new BlockingThread { prefix, func, user_data } };
    bthread->start();
    return bthread;
}

bool BlockingThread::is_current_thread() const
{
    return std::this_thread::get_id() == thread_.get_id();
}

void BlockingThread::start()
{
    std::promise<void> promise;
    auto future = promise.get_future();
    thread_ = std::thread {
        [&promise, this]() {
            main_loop(promise);
        }
    };
    future.get();
}

void BlockingThread::main_loop(std::promise<void>& promise)
{
    set_thread_name();
    register_to_thread_watcher();
    promise.set_value();
    user_func_(std::bind(&BlockingThread::i_am_alive, this), user_data_);
    unregister_from_thread_watcher();
}

void BlockingThread::register_to_thread_watcher()
{
    ThreadWatcher::instance()->add(name_, std::this_thread::get_id());
}

void BlockingThread::unregister_from_thread_watcher()
{
    ThreadWatcher::instance()->remove(name_);
}

void BlockingThread::i_am_alive()
{
    constexpr int64_t k1Second = 1'000;
    int64_t now = ltlib::steady_now_ms();
    if (now - last_report_time_ > k1Second) {
        last_report_time_ = now;
        ThreadWatcher::instance()->report_alive(name_);
    }
}

void BlockingThread::set_thread_name()
{
    ::set_current_thread_name(name_.c_str());
}


std::unique_ptr<TaskThread> TaskThread::create(const std::string& prefix)
{
    if (prefix.empty()) {
        return nullptr;
    }
    std::unique_ptr<TaskThread> tthread { new TaskThread { prefix } };
    tthread->start();
    return tthread;
}

TaskThread::TaskThread(const std::string& prefix)
    : last_report_time_ { ltlib::steady_now_ms() }
{
    std::stringstream ss;
    ss << prefix << '-' << std::hex << (int64_t)this;
    name_ = ss.str();
}


TaskThread::~TaskThread()
{
    {
        std::lock_guard lock { mutex_ };
        stoped_ = true;
    }
    cv_.notify_one();
    // task thread可能没有start()就析构了，所以需要检查joinable()
    if (thread_.joinable()) {
        thread_.join();
    }
}

void TaskThread::post(const Task& task)
{
    {
        std::lock_guard<std::mutex> lock { mutex_ };
        tasks_.push_back(task);
        wakeup_ = true;
    }
    cv_.notify_one();
}

TaskThread::TimerID TaskThread::post_delay(TimeDelta delta_time, const Task& task)
{
    Timestamp when = Timestamp::now() + delta_time;
    {
        std::lock_guard<std::mutex> lock { mutex_ };
        for (;;) {
            auto iter = delay_tasks_.find(when);
            if (iter == delay_tasks_.end()) {
                delay_tasks_.emplace(when, task);
                break;
            }
            when = when + 1_us;
        }
    }
    cv_.notify_one();
    return when.microseconds();
}

void TaskThread::start()
{
    std::promise<void> promise;
    auto future = promise.get_future();
    thread_ = std::thread { [&promise, this]() {
        main_loop(promise);
    } };
    future.get();
}

void TaskThread::main_loop(std::promise<void>& promise)
{
    set_thread_name();
    register_to_thread_watcher();
    promise.set_value();

    while (!stoped_) {
        i_am_alive();
        auto old_tasks = get_pending_tasks();
        auto [delay_tasks, sleep_for] = get_timeup_delay_tasks();
        // auto proactor_tasks = get_proactor_tasks();

        if (old_tasks.empty() && delay_tasks.empty()) { // && proactor_tasks.empty())
            std::unique_lock lock { mutex_ };
            wakeup_.store(false, std::memory_order::memory_order_relaxed);
            cv_.wait_for(lock, std::chrono::microseconds { sleep_for.value() }, [this]() { return wakeup_.load(std::memory_order::memory_order_relaxed); });
            continue;
        }

        for (auto&& task : delay_tasks) {
            task();
        }
        for (auto&& task : old_tasks) {
            task();
        }
        // for (auto&& task : proactor_tasks) {
        //     task();
        // }
    }
    unregister_from_thread_watcher();
}

void TaskThread::wake_up()
{
    {
        std::lock_guard lock { mutex_ };
        wakeup_ = true;
    }
    cv_.notify_one();
}

void TaskThread::i_am_alive()
{
    constexpr int64_t k1Second = 1'000;
    int64_t now = ltlib::steady_now_ms();
    if (now - last_report_time_ > k1Second) {
        last_report_time_ = now;
        ThreadWatcher::instance()->report_alive(name_);
    }
}

void TaskThread::register_to_thread_watcher()
{
    ThreadWatcher::instance()->add(name_, std::this_thread::get_id());
}

void TaskThread::unregister_from_thread_watcher()
{
    ThreadWatcher::instance()->remove(name_);
}

void TaskThread::set_thread_name()
{
    ::set_current_thread_name(name_.c_str());
}

std::deque<TaskThread::Task> TaskThread::get_pending_tasks()
{
    std::lock_guard lock { mutex_ };
    return std::move(tasks_);
}

std::tuple<std::vector<TaskThread::Task>, TimeDelta> TaskThread::get_timeup_delay_tasks()
{
    std::vector<Task> tasks;
    auto now = Timestamp::now();
    std::lock_guard lock { mutex_ };
    auto iter = delay_tasks_.begin();
    while (iter != delay_tasks_.end() && iter->first <= now) {
        tasks.push_back(iter->second);
        iter = delay_tasks_.erase(iter);
    }

    if (!delay_tasks_.empty()) {
        return { tasks, delay_tasks_.begin()->first - now };
    } else {
        return { tasks, TimeDelta { 10'000 } };
    }
}

bool TaskThread::is_current_thread()
{
    return std::this_thread::get_id() == thread_.get_id();
}

void TaskThread::wake()
{
    wake_up();
}

bool TaskThread::is_running()
{
    return wakeup_.load(std::memory_order::memory_order_relaxed);
}

void TaskThread::invokeInternal(const Task& task)
{
    std::promise<void> promise;
    post([&promise, task]() {
        task();
        promise.set_value();
    });
    promise.get_future().get();
}

void TaskThread::cancel(TimerID timer)
{
    std::lock_guard<std::mutex> lock(mutex_);
    delay_tasks_.erase(timer);
}

} // namespace ltlib