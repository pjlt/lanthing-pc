#pragma once
#include <string>
#include <map>
#include <memory>
#include <rtc/rtc.h>
#include <ltlib/io/ioloop.h>
#include <ltlib/io/client.h>
#include <ltlib/threads.h>
#include <graphics/encoder/video_encoder.h>
#include <graphics/capturer/video_capturer.h>
#include "message_handler.h"
#include "session_change_observer.h"
#include "display_setting.h"

namespace lt
{

namespace worker
{

class Worker
{
public:
    struct Params
    {
        std::string name;
        uint32_t width;
        uint32_t height;
        uint32_t refresh_rate;
        std::vector<rtc::VideoCodecType> codecs;
    };

public:
    static std::unique_ptr<Worker> create(std::map<std::string, std::string> options);
    ~Worker();
    void wait();

private:
    Worker(const Params& params);
    bool init();
    bool init_pipe_client();
    void negotiate_parameters();
    void main_loop(const std::function<void()>& i_am_alive);
    void stop();
    bool register_message_handler(uint32_t type, const MessageHandler& msg);
    void dispatch_service_message(uint32_t type, const std::shared_ptr<google::protobuf::MessageLite>& msg);
    bool send_pipe_message(uint32_t type, const std::shared_ptr<google::protobuf::MessageLite>& msg);
    void print_stats();
    void check_timeout();
    void on_captured_video_frame(std::shared_ptr<google::protobuf::MessageLite> frame);

    // pipe message handlers
    void on_pipe_message(uint32_t type, std::shared_ptr<google::protobuf::MessageLite> msg);
    void on_pipe_disconnected();
    void on_pipe_reconnecting();
    void on_pipe_connected();
    void on_start_working(const std::shared_ptr<google::protobuf::MessageLite>& msg);
    void on_stop_working(const std::shared_ptr<google::protobuf::MessageLite>& msg);
    void on_keep_alive(const std::shared_ptr<google::protobuf::MessageLite>& msg);

private:
    const uint32_t client_width_;
    const uint32_t client_height_;
    const uint32_t client_refresh_rate_;
    const std::vector<rtc::VideoCodecType> client_codec_types_;
    const std::string pipe_name_;
    bool connected_to_service_ = false;
    std::unique_ptr<SessionChangeObserver> session_observer_;
    std::map<uint32_t, MessageHandler> msg_handlers_;
    DisplaySetting negotiated_display_setting_;
    rtc::VideoCodecType negotiated_video_codec_type_ = rtc::VideoCodecType::Unknown;
    VideoEncoder::Backend negotiated_video_codec_beckend_ = VideoEncoder::Backend::Unknown;
    std::shared_ptr<google::protobuf::MessageLite> negotiated_params_;
    std::unique_ptr<ltlib::IOLoop> ioloop_;
    std::unique_ptr<ltlib::Client> pipe_client_;
    std::unique_ptr<ltlib::BlockingThread> thread_;
    int64_t last_time_received_from_service_;
    std::unique_ptr<lt::VideoCapturer> video_capturer_;
};

} // namespace worker

} // namespace lt