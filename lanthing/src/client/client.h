#pragma once
#include <cstdint>
#include <memory>
#include <map>
#include <string>
#include <mutex>
#include <condition_variable>

#include <ltrtc/ltclient.h>
#include <ltlib/io/client.h>
#include <ltlib/io/ioloop.h>
#include <ltlib/threads.h>

#include <client/platforms/pc_sdl.h>
#include <client/graphics/video.h>
#include <client/input/input.h>

namespace lt
{

namespace cli
{

struct SignalingParams
{
    SignalingParams(const std::string& _client_id, const std::string& _room_id, const std::string& _addr, uint16_t _port)
        : client_id(_client_id)
        , room_id(_room_id)
        , addr(_addr)
        , port(_port)
    {
    }
    std::string client_id;
    std::string room_id;
    std::string addr;
    uint16_t port;
};

class Client
{
public:
    struct Params
    {
        std::string client_id;
        std::string room_id;
        std::string auth_token;
        std::string user;
        std::string pwd;
        std::string signaling_addr;
        uint16_t signaling_port;
        std::string codec;
        uint32_t width;
        uint32_t height;
        uint32_t screen_refresh_rate;
        bool enable_driver_input;
        bool enable_gamepad;
    };

public:
    static std::unique_ptr<Client> create(std::map<std::string, std::string> options);
    ~Client();
    void wait();

private:
    Client(const Params& params);
    bool init();
    void main_loop(const std::function<void()>& i_am_alive);
    void on_platform_render_target_reset();
    void on_platform_exit();
    void stop_wait();

    // 信令.
    void on_signaling_net_message(uint32_t type, std::shared_ptr<google::protobuf::MessageLite> msg);
    void on_signaling_disconnected();
    void on_signaling_reconnecting();
    void on_signaling_connected();
    void on_join_room_ack(std::shared_ptr<google::protobuf::MessageLite> msg);
    void on_signaling_message(std::shared_ptr<google::protobuf::MessageLite> msg);
    void on_signaling_message_ack(std::shared_ptr<google::protobuf::MessageLite> msg);

    // ltrtc::LTClient
    bool init_ltrtc();
    void on_ltrtc_data(const std::shared_ptr<uint8_t>& data, uint32_t size, bool is_reliable);
    void on_ltrtc_video_frame(const ltrtc::VideoFrame& frame);
    void on_ltrtc_audio_data(uint32_t bits_per_sample, uint32_t sample_rate, uint32_t number_of_channels, const void* audio_data, uint32_t size);
    void on_ltrtc_connected(/*connection info*/);
    void on_ltrtc_conn_changed(/*old_conn_info, new_conn_info*/);
    void on_ltrtc_failed();
    void on_ltrtc_disconnected();
    void on_ltrtc_signaling_message(const std::string& key, const std::string& value);

    // 数据通道.
    void dispatch_remote_message(uint32_t type, const std::shared_ptr<google::protobuf::MessageLite>& msg);
    void send_keep_alive();
    bool send_message_to_host(uint32_t type, const std::shared_ptr<google::protobuf::MessageLite>& msg, bool reliable);
    void on_start_transmission_ack(const std::shared_ptr<google::protobuf::MessageLite>& msg);

private:
    std::string auth_token_;
    std::string p2p_username_;
    std::string p2p_password_;
    SignalingParams signaling_params_;
    Input::Params input_params_ {};
    Video::Params video_params_;
    std::unique_ptr<Video> video_module_;
    std::unique_ptr<Input> input_module_;
    std::unique_ptr<ltlib::IOLoop> ioloop_;
    std::unique_ptr<ltlib::Client> signaling_client_;
    std::unique_ptr<ltrtc::LTClient> ltrtc_client_;
    std::unique_ptr<PcSdl> sdl_;
    std::unique_ptr<ltlib::BlockingThread> main_thread_;
    std::unique_ptr<ltlib::TaskThread> hb_thread_;
    std::condition_variable exit_cv_;
    std::mutex exit_mutex_;
    bool should_exit_ = true;
};

} // namespace cli

} // namespace lt