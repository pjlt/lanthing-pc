#pragma once
#include <cstdint>
#include <memory>
#include <vector>
#include <optional>
#include <limits>
#include <ltproto/peer2peer/capture_video_frame.pb.h>
#include <ltrtc/lttypes.h>

namespace lt
{

namespace svc
{

class D3D11Provider;

class VideoEncoder
{
public:
    enum class Backend
    {
        Unknown,
        NvEnc,
        IntelMediaSDK,
        Amf,
    };

    enum class FrameType
    {
        IFrame,
        PFrame,
    };

    struct EncodedFrame : ltrtc::VideoFrame
    {
        bool is_black_frame = false;
    };

    struct InitParams
    {
        void* context = nullptr;
        Backend backend = Backend::Unknown;
        ltrtc::VideoCodecType codec_type = ltrtc::VideoCodecType::H264;
        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t bitrate_bps = 0;

        bool validate() const;
    };

    struct ReconfigureParams
    {
        std::optional<uint32_t> bitrate_bps;
        std::optional<uint32_t> fps;
    };

public:
    static std::unique_ptr<VideoEncoder> create(const InitParams& params);
    virtual ~VideoEncoder();
    virtual void reconfigure(const ReconfigureParams& params) = 0;
    EncodedFrame encode(std::shared_ptr<ltproto::peer2peer::CaptureVideoFrame> input_frame, bool force_idr);

protected:
    VideoEncoder(void* d3d11_device);
    virtual EncodedFrame encode_one_frame(void* input_frame, bool force_idr) = 0;

private:
    void* d3d11_device_;
};

} // namespace svc

} // namespace lt