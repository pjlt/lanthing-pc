#pragma once
#include <memory>
#include <future>
#include <functional>
#include <atomic>
//#include <mutex>
//#include <condition_variable>
#include <ltproto/peer2peer/capture_video_frame.pb.h>
#include <ltlib/threads.h>

namespace lt
{

class VideoCapturer
{
public:
	enum class Backend
	{
		Dxgi,
	};
	using OnFrame = std::function<void(std::shared_ptr<ltproto::peer2peer::CaptureVideoFrame>)>;

	struct Params
	{
        Backend backend;
        OnFrame on_frame;
	};

public:
    static std::unique_ptr<VideoCapturer> create(const Params& params);
    virtual ~VideoCapturer();
    void stop();

protected:
    VideoCapturer();
    virtual bool pre_init() = 0;
    virtual std::shared_ptr<ltproto::peer2peer::CaptureVideoFrame> capture_one_frame() = 0;
    //virtual void done_with_frame() = 0;
    virtual void wait_for_vblank() = 0;

private:
    bool init();
    void main_loop(const std::function<void()>& i_am_alive);

private:
    OnFrame on_frame_;
    std::unique_ptr<ltlib::BlockingThread> thread_;
    //std::mutex mutex_;
    //std::condition_variable cv_;
    uint64_t frame_no_ = 0;
    std::atomic<bool> stoped_ { true };
    std::unique_ptr<std::promise<void>> stop_promise_;
};

} // namespace lt