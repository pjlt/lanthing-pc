#pragma once
#include <cstdint>

#include <atomic>
#include <future>
#include <memory>
#include <mutex>
#include <string>

#include <google/protobuf/message_lite.h>

#include <ltlib/io/ioloop.h>
#include <ltlib/io/server.h>
#include <ltlib/threads.h>
#include <ltproto/ltproto.h>
#include <transport/transport.h>

namespace lt {

namespace svc {

class WorkerProcess {
public:
    struct Params {
        std::string pipe_name;
        std::function<void()> on_stoped;
        std::string path;
        uint32_t client_width;
        uint32_t client_height;
        uint32_t client_refresh_rate;
        std::vector<lt::VideoCodecType> client_codecs;
    };

public:
    static std::unique_ptr<WorkerProcess> create(const Params& params);
    ~WorkerProcess();

private:
    WorkerProcess(const Params& params);
    void start();
    void main_loop(std::promise<void>& promise, const std::function<void()>& i_am_alive);
    bool launch_worker_process();
    void wait_for_worker_process(const std::function<void()>& i_am_alive);

private:
    std::function<void(uint32_t, std::shared_ptr<google::protobuf::MessageLite>)> on_message_;
    std::function<void()> on_stoped_;
    std::string path_;
    std::string pipe_name_;
    uint32_t client_width_;
    uint32_t client_height_;
    uint32_t client_refresh_rate_;
    std::vector<lt::VideoCodecType> client_codecs_;
    bool run_as_win_service_;
    std::mutex mutex_;
    std::unique_ptr<ltlib::BlockingThread> thread_;
    std::atomic<bool> stoped_{true};
    void* process_handle_ = nullptr;
    void* thread_handle_ = nullptr;
    ltproto::Parser parser_;
};

} // namespace svc

} // namespace lt