#pragma once
#include <ltrtc/lttypes.h>

#include <cstdint>
#include <memory>
#include <functional>

namespace ltrtc
{

struct LTClientConfig
{
    VideoCodecType video_codec_type;
    const char* username;
    const char* password;
    OnData on_data;
    OnVideo on_video;
    OnAudio on_audio;
    OnConnected on_connected;
    OnConnChanged on_conn_changed;
    OnFailed on_failed;
    OnDisconnected on_disconnected;
    OnSignalingMessage on_signaling_message;
};

class LTClient
{
public:
    static std::unique_ptr<LTClient> create(LTClientConfig&& config);
    virtual ~LTClient() { }

    virtual bool connect() = 0;
    virtual void close() = 0;
    virtual bool send_data(const std::shared_ptr<uint8_t>& data, uint32_t size, bool is_reliable) = 0;
    virtual void on_signaling_message(const std::string& key, const std::string& value) = 0;

protected:
    LTClient() = default;
};

} // namespace ltrtc
