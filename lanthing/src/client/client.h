#pragma once
#include <condition_variable>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <string>

#include <ltlib/io/client.h>
#include <ltlib/io/ioloop.h>
#include <ltlib/threads.h>
#include <transport/transport.h>

#include <audio/player/audio_player.h>
#include <graphics/drpipeline/video_decode_render_pipeline.h>
#include <inputs/capturer/input_capturer.h>
#include <platforms/pc_sdl.h>

namespace lt {

namespace cli {

struct SignalingParams {
    SignalingParams(const std::string& _client_id, const std::string& _room_id,
                    const std::string& _addr, uint16_t _port)
        : client_id(_client_id)
        , room_id(_room_id)
        , addr(_addr)
        , port(_port) {}
    std::string client_id;
    std::string room_id;
    std::string addr;
    uint16_t port;
};

class Client {
public:
    struct Params {
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
        uint32_t audio_freq;
        uint32_t audio_channels;
        bool enable_driver_input;
        bool enable_gamepad;
        std::vector<std::string> reflex_servers;
    };

public:
    static std::unique_ptr<Client> create(std::map<std::string, std::string> options);
    ~Client();
    void wait();

private:
    Client(const Params& params);
    bool init();
    void mainLoop(const std::function<void()>& i_am_alive);
    void onPlatformRenderTargetReset();
    void onPlatformExit();
    void stopWait();
    void postTask(const std::function<void()>& task);
    void postDelayTask(int64_t delay_ms, const std::function<void()>& task);

    // 信令.
    void onSignalingNetMessage(uint32_t type, std::shared_ptr<google::protobuf::MessageLite> msg);
    void onSignalingDisconnected();
    void onSignalingReconnecting();
    void onSignalingConnected();
    void onJoinRoomAck(std::shared_ptr<google::protobuf::MessageLite> msg);
    void onSignalingMessage(std::shared_ptr<google::protobuf::MessageLite> msg);
    void onSignalingMessageAck(std::shared_ptr<google::protobuf::MessageLite> msg);

    // transport
    bool initTransport();
    void onTpData(const uint8_t* data, uint32_t size, bool is_reliable);
    void onTpVideoFrame(const lt::VideoFrame& frame);
    void onTpAudioData(const lt::AudioData& audio_data);
    void onTpConnected(/*connection info*/);
    void onTpConnChanged(/*old_conn_info, new_conn_info*/);
    void onTpFailed();
    void onTpDisconnected();
    void onTpSignalingMessage(const std::string& key, const std::string& value);

    // 数据通道.
    void dispatchRemoteMessage(uint32_t type,
                               const std::shared_ptr<google::protobuf::MessageLite>& msg);
    void sendKeepAlive();
    bool sendMessageToHost(uint32_t type, const std::shared_ptr<google::protobuf::MessageLite>& msg,
                           bool reliable);
    void onStartTransmissionAck(const std::shared_ptr<google::protobuf::MessageLite>& msg);

private:
    std::string auth_token_;
    std::string p2p_username_;
    std::string p2p_password_;
    SignalingParams signaling_params_;
    InputCapturer::Params input_params_{};
    VideoDecodeRenderPipeline::Params video_params_;
    AudioPlayer::Params audio_params_{};
    std::vector<std::string> reflex_servers_;
    std::unique_ptr<VideoDecodeRenderPipeline> video_pipeline_;
    std::unique_ptr<InputCapturer> input_capturer_;
    std::unique_ptr<AudioPlayer> audio_player_;
    std::mutex ioloop_mutex_;
    std::unique_ptr<ltlib::IOLoop> ioloop_;
    std::unique_ptr<ltlib::Client> signaling_client_;
    std::unique_ptr<lt::tp::Client> tp_client_;
    std::unique_ptr<PcSdl> sdl_;
    std::unique_ptr<ltlib::BlockingThread> main_thread_;
    std::unique_ptr<ltlib::TaskThread> hb_thread_;
    std::condition_variable exit_cv_;
    std::mutex exit_mutex_;
    bool should_exit_ = true;
};

} // namespace cli

} // namespace lt