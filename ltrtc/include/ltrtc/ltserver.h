#pragma once
#include <ltrtc/lttypes.h>

#include <cstdint>
#include <memory>
#include <functional>

namespace ltrtc
{

struct LTServerConfig
{
    VideoCodecType video_codec_type;
    const char* username;
    const char* password;
    OnData on_data;
    OnConnected on_accepted;
    OnConnChanged on_conn_changed;
    OnFailed on_failed;
    OnDisconnected on_disconnected;
    OnSignalingMessage on_signaling_message;
};

class LTServer
{
public:
    static std::unique_ptr<LTServer> create(LTServerConfig&& config);
    virtual ~LTServer() { }

    virtual void close() = 0;
    virtual bool send_data(const std::shared_ptr<uint8_t>& data, uint32_t size, bool is_reliable) = 0;
    virtual bool send_audio(const std::shared_ptr<uint8_t>& data, uint32_t size) = 0;
    virtual bool send_video(const VideoFrame& frame) = 0;
    virtual void on_signaling_message(const std::string& key, const std::string& value) = 0;

protected:
    LTServer() = default;
};

} // namespace ltrtc
