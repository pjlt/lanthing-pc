#include <ltlib/io/ioloop.h>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <g3log/g3log.hpp>
#include <uv.h>

namespace ltlib
{

class IOLoopImpl
{
public:
    IOLoopImpl() = default; //??
    ~IOLoopImpl() = default; //??
    bool init();
    void run(const std::function<void()>& i_am_alive);
    void stop();
    void post(const std::function<void()>& task);
    void post_delay(int64_t delay_ms, const std::function<void()>& task);
    bool is_current_thread() const;
    uv_loop_t* context();

private:
    static void consume_tasks(uv_async_t* handle);

private:
    uv_loop_t uvloop_ {};
    uv_async_t close_handle_ {};
    uv_async_t task_handle_ {};
    uv_timer_t alive_handle_ {};
    std::mutex mutex_;
    std::condition_variable cv_;
    bool stoped_ = false;
    std::vector<std::function<void()>> tasks_;
    std::thread::id tid_;
};

std::unique_ptr<IOLoop> IOLoop::create()
{
    auto impl = std::make_shared<IOLoopImpl>();
    if (!impl->init()) {
        return nullptr;
    }
    std::unique_ptr<IOLoop> loop { new IOLoop };
    loop->impl_ = std::move(impl);
    return loop;
}

void IOLoop::run(const std::function<void()>& i_am_alive)
{
    impl_->run(i_am_alive);
}

void IOLoop::stop()
{
    impl_->stop();
}

void IOLoop::post(const std::function<void()>& task)
{
    impl_->post(task);
}

void IOLoop::post_delay(int64_t delay_ms, const std::function<void()>& task)
{
    impl_->post_delay(delay_ms, task);
}

bool IOLoop::is_current_thread() const
{
    return impl_->is_current_thread();
}

bool IOLoop::is_not_current_thread() const
{
    return !impl_->is_current_thread();
}

void* IOLoop::context()
{
    return impl_->context();
}

bool IOLoopImpl::init()
{
    int ret = uv_loop_init(&uvloop_);
    if (ret != 0) {
        LOG(WARNING) << "uv_loop_init failed: " << ret;
        return false;
    }
    return true;
}

void IOLoopImpl::run(const std::function<void()>& i_am_alive)
{
    constexpr uint32_t k700ms = 700;
    uv_timer_init(&uvloop_, &alive_handle_);
    // NOTE: 这个copy内存泄露了，但是不用管
    auto copy_i_am_alive = new std::function<void()>;
    *copy_i_am_alive = i_am_alive;
    alive_handle_.data = copy_i_am_alive;
    uv_timer_start(
        &alive_handle_, [](uv_timer_t* handler) {
            auto i_am_alive = (std::function<void()>*)handler->data;
            i_am_alive->operator()();
        },
        k700ms, k700ms);

    uv_async_init(
        &uvloop_,
        &close_handle_,
        [](uv_async_t* handle) { uv_stop(handle->loop); });
    uv_async_init(
        &uvloop_,
        &task_handle_,
        &IOLoopImpl::consume_tasks);
    task_handle_.data = this;
    stoped_ = false;
    tid_ = std::this_thread::get_id();
    uv_run(&uvloop_, UV_RUN_DEFAULT);
    // 发送信号，表示已经退出循环
    {
        std::lock_guard<std::mutex> lock { mutex_ };
        stoped_ = true;
    }
    cv_.notify_one();
}

void IOLoopImpl::stop()
{
    // 1. 向main_loop线程发送stop信号
    uv_async_send(&close_handle_);
    // 2. 等待main_loop发送信号
    std::unique_lock<std::mutex> lock { mutex_ };
    cv_.wait(lock, [this]() { return stoped_; });
    // 3. 遍历所有未关闭的handle，关闭它们
    uv_walk(
        &uvloop_, [](uv_handle_t* handle, void* arg) {
            if (!uv_is_closing(handle)) {
                uv_close(handle, [](uv_handle_t* h) {});
            }
        },
        nullptr);
    // 4. 再uv_run一次，让第3步的handle能执行自己的close callback
    uv_run(&uvloop_, UV_RUN_DEFAULT);
    // 5. 回收uvloop_内部资源
    uv_loop_close(&uvloop_);
}

void IOLoopImpl::post(const std::function<void()>& task)
{
    {
        std::lock_guard<std::mutex> lock { mutex_ };
        tasks_.push_back(task);
    }
    // 对同一个uv_async_t多次调用uv_async_send是冇问题哒！
    uv_async_send(&task_handle_);
}

void IOLoopImpl::post_delay(int64_t delay_ms, const std::function<void()>& task)
{
    // 整个libuv只有uv_async_send()是线程安全的，所以我们要再包一层，把uv_timer_init()、uv_timer_start()扔到libuv的线程去跑
    auto user_task_copied = new std::function<void()> { task };
    auto delayed_task = [delay_ms, user_task_copied, this]() {
        auto timer = new uv_timer_t;
        uv_timer_init(&uvloop_, timer);
        timer->data = user_task_copied;
        uv_timer_start(
            timer, [](uv_timer_t* handle) {
                auto user_task = reinterpret_cast<std::function<void()>*>(handle->data);
                user_task->operator()();
                delete user_task;
                // handle只能在uv_close_cb里删除
                uv_timer_stop(handle);
                uv_close((uv_handle_t*)handle, [](uv_handle_t* handle) {
                    auto timer_handle = (uv_timer_t*)handle;
                    delete timer_handle;
                });
            },
            delay_ms, 0);
    };
    post(delayed_task);
}

bool IOLoopImpl::is_current_thread() const
{
    return tid_ == std::this_thread::get_id();
}

uv_loop_t* IOLoopImpl::context()
{
    return &uvloop_;
}

void IOLoopImpl::consume_tasks(uv_async_t* handle)
{
    IOLoopImpl* that = (IOLoopImpl*)handle->data;
    std::vector<std::function<void()>> tasks;
    {
        std::lock_guard<std::mutex> lock { that->mutex_ };
        tasks.swap(that->tasks_);
    }
    for (auto& task : tasks) {
        task();
    }
}

} // namespace ltlib