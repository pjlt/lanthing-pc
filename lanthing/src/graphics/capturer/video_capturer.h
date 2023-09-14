#pragma once
#include <atomic>
#include <functional>
#include <future>
#include <memory>

#include <ltproto/peer2peer/capture_video_frame.pb.h>

#include <ltlib/threads.h>

namespace lt {

class VideoCapturer {
public:
    enum class Backend {
        Dxgi,
    };
    using OnFrame = std::function<void(std::shared_ptr<ltproto::peer2peer::CaptureVideoFrame>)>;

    struct Params {
        Backend backend;
        OnFrame on_frame;
    };

public:
    static std::unique_ptr<VideoCapturer> create(const Params& params);
    virtual ~VideoCapturer();
    void start();
    void stop();
    virtual void releaseFrame(const std::string& name) = 0;
    virtual Backend backend() const = 0;
    virtual int64_t luid() { return -1; }

protected:
    VideoCapturer();
    virtual bool preInit() = 0;
    virtual std::shared_ptr<ltproto::peer2peer::CaptureVideoFrame> captureOneFrame() = 0;
    // virtual void done_with_frame() = 0;
    virtual void waitForVblank() = 0;

private:
    bool init();
    void mainLoop(const std::function<void()>& i_am_alive);

private:
    OnFrame on_frame_;
    std::unique_ptr<ltlib::BlockingThread> thread_;
    // std::mutex mutex_;
    // std::condition_variable cv_;
    uint64_t frame_no_ = 0;
    std::atomic<bool> stoped_{true};
    std::unique_ptr<std::promise<void>> stop_promise_;
};

} // namespace lt