#include "video.h"
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <g3log/g3log.hpp>
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <SDL_syswm.h>

#include <ltlib/threads.h>
#include <ltlib/times.h>

#include "d3d11_pipeline.h"
#include "ct_smoother.h"

namespace lt
{

namespace cli
{

using namespace std::chrono_literals;

class VideoImpl
{
public:
    VideoImpl(const Video::Params& params);
    ~VideoImpl();
    bool init();
    void reset_deocder_renderer();
    Video::Action submit(const ltrtc::VideoFrame& frame);

private:
    void decode_loop(const std::function<void()>& i_am_alive);
    void render_loop(const std::function<void()>& i_am_alive);

private:
    const uint32_t width_;
    const uint32_t height_;
    const uint32_t screen_refresh_rate_;
    const ltrtc::VideoCodecType codec_type_;
    std::function<void(uint32_t, std::shared_ptr<google::protobuf::MessageLite>, bool)> send_message_to_host_;
    HWND hwnd_;

    bool signal_ = false;
    std::mutex wait_for_mtx_;
    std::condition_variable wait_for_frames_;
    std::atomic<bool> request_i_frame_ = false;
    std::vector<ltrtc::VideoFrame> encoded_frames_;

    std::unique_ptr<D3D11Pipeline> d3d11_pipe_line_;
    CTSmoother smoother_;
    std::atomic<bool> stoped_ { true };
    std::unique_ptr<ltlib::BlockingThread> decode_thread_;
    std::unique_ptr<ltlib::BlockingThread> render_thread_;
};


VideoImpl::VideoImpl(const Video::Params& params)
    : width_ { params.width }
    , height_ { params.height }
    , screen_refresh_rate_ { params.screen_refresh_rate }
    , codec_type_ { params.codec_type }
    , send_message_to_host_ { params.send_message_to_host }
{
    SDL_SysWMinfo info {};
    SDL_VERSION(&info.version);
    SDL_GetWindowWMInfo(params.sdl->window(), &info);
    hwnd_ = info.info.win.window;
}

VideoImpl::~VideoImpl()
{
    stoped_ = true;
    decode_thread_.reset();
    render_thread_.reset();
    d3d11_pipe_line_.reset();
}

bool VideoImpl::init()
{
    d3d11_pipe_line_ = std::make_unique<D3D11Pipeline>();
    if (!d3d11_pipe_line_->init(0)) {
        LOG(WARNING) << "Failed to initialize d3d11 pipeline on apdater " << 0;
        d3d11_pipe_line_ = nullptr;
        return false;
    }
    if (!d3d11_pipe_line_->setupRender((HWND)hwnd_, width_, height_)) {
        LOGF(WARNING, "Failed to setup d3d11 pipeline[render] on 0x%08x", hwnd_);
        d3d11_pipe_line_ = nullptr;
        return false;
    }
    if (!d3d11_pipe_line_->setupDecoder(Format::H265_NV12)) {
        LOG(WARNING) << "Failed to setup d3d11 pipeline[decoder]";
        return false;
    }
    smoother_.clear();
    stoped_ = false;
    decode_thread_ = ltlib::BlockingThread::create(
        "decode",
        [this](const std::function<void()>& i_am_alive, void*) {
            decode_loop(i_am_alive);
        },
        nullptr);
    render_thread_ = ltlib::BlockingThread::create(
        "render",
        [this](const std::function<void()>& i_am_alive, void*) {
            render_loop(i_am_alive);
        },
        nullptr);
    return true;
}

void VideoImpl::reset_deocder_renderer()
{
    LOG(FATAL) << "reset_deocder_renderer() not implemented";
}

Video::Action VideoImpl::submit(const ltrtc::VideoFrame& frame)
{
    {
        std::unique_lock<std::mutex> lock(wait_for_mtx_);
        encoded_frames_.emplace_back(frame);
        signal_ = true;
        lock.unlock();
    }
    bool request_i_frame = request_i_frame_.exchange(false);
    return request_i_frame ? Video::Action::REQUEST_KEY_FRAME
                           : Video::Action::NONE;
}

void VideoImpl::decode_loop(const std::function<void()>& i_am_alive)
{
    while (!stoped_) {
        i_am_alive();
        std::vector<ltrtc::VideoFrame> frames;
        {
            std::unique_lock<std::mutex> lock(wait_for_mtx_);
            if (encoded_frames_.empty()) {
                wait_for_frames_.wait_for(lock, 5ms, [this]() { return signal_; });
                signal_ = false;
            }
            frames.swap(encoded_frames_);
        }
        if (frames.empty()) {
            continue;
        }
        for (auto frame : frames) {
            auto resouce_id = d3d11_pipe_line_->decode(frame.data, frame.size);
            if (resouce_id < 0) {
                LOG(WARNING) << "failed to call decode(), reqesut i frame";
                request_i_frame_ = true;
                break;
            }
            CTSmoother::Frame f;
            f.no = resouce_id;
            f.capture_time = frame.capture_timestamp_us;
            f.at_time = ltlib::steady_now_us();
            smoother_.push(f);
        }
    }
}

void VideoImpl::render_loop(const std::function<void()>& i_am_alive)
{
    uint64_t cur_time_us = 0;
    uint64_t vsync_time_us = 0;
    uint64_t render_time_us = 0;
    while (!stoped_) {
        i_am_alive();
        std::this_thread::sleep_for(1ms);
        cur_time_us = ltlib::steady_now_us();
        if (cur_time_us > vsync_time_us) {
            vsync_time_us = d3d11_pipe_line_->nextVsyncTime(cur_time_us);
            render_time_us = vsync_time_us - 5'000;
        }
        auto frame = smoother_.get(cur_time_us);
        if (frame < 0) {
            continue;
        }
        d3d11_pipe_line_->render(frame);
        smoother_.pop();
    }
}


Video::Params::Params(ltrtc::VideoCodecType _codec_type, uint32_t _width, uint32_t _height, uint32_t _screen_refresh_rate, std::function<void(uint32_t, std::shared_ptr<google::protobuf::MessageLite>, bool)> send_message)
    : codec_type(_codec_type)
    , width(_width)
    , height(_height)
    , screen_refresh_rate(_screen_refresh_rate)
    , send_message_to_host(send_message)
{
}

bool Video::Params::validate() const
{
    if (codec_type == ltrtc::VideoCodecType::Unknown || sdl == nullptr || send_message_to_host == nullptr) {
        return false;
    } else {
        return true;
    }
}

std::unique_ptr<Video> Video::create(const Params& params)
{
    if (!params.validate()) {
        LOG(FATAL) << "Create video module failed: invalid parameter";
        return nullptr;
    }
    auto impl = std::make_shared<VideoImpl>(params);
    if (!impl->init()) {
        return nullptr;
    }
    std::unique_ptr<Video> video { new Video };
    video->impl_ = std::move(impl);
    return video;
}

void Video::reset_decoder_renderer()
{
    impl_->reset_deocder_renderer();
}

Video::Action Video::submit(const ltrtc::VideoFrame& frame)
{
    return impl_->submit(frame);
}

} // namespace cli

} // namespace lt