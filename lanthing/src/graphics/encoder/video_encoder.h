#pragma once
#include <array>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <vector>

#include <ltproto/peer2peer/capture_video_frame.pb.h>

#include <transport/transport.h>

namespace lt {

class VideoEncoder {
public:
    enum class Backend {
        Unknown,
        NvEnc,
        IntelMediaSDK,
        Amf,
    };

    enum class FrameType {
        IFrame,
        PFrame,
    };

    struct EncodedFrame : lt::VideoFrame {
        bool is_black_frame = false;
        std::shared_ptr<uint8_t> internal_data;
    };

    struct InitParams {
        Backend backend = Backend::Unknown;
        int64_t luid;
        lt::VideoCodecType codec_type = lt::VideoCodecType::H264;
        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t bitrate_bps = 0;

        bool validate() const;
    };

    struct ReconfigureParams {
        std::optional<uint32_t> bitrate_bps;
        std::optional<uint32_t> fps;
    };

    struct Ability {
        Backend backend;
        lt::VideoCodecType codec_type;
    };

public:
    static std::unique_ptr<VideoEncoder> create(const InitParams& params);
    virtual ~VideoEncoder();
    virtual void reconfigure(const ReconfigureParams& params) = 0;
    void requestKeyframe();
    EncodedFrame encode(std::shared_ptr<ltproto::peer2peer::CaptureVideoFrame> input_frame);

    static std::vector<Ability> checkEncodeAbilities(uint32_t width, uint32_t height);
    static std::vector<Ability> checkEncodeAbilitiesWithLuid(int64_t luid, uint32_t width,
                                                             uint32_t height);

protected:
    VideoEncoder(void* d3d11_dev, void* d3d11_ctx);
    bool needKeyframe();
    virtual EncodedFrame encodeFrame(void* input_frame) = 0;

private:
    void* d3d11_dev_ = nullptr;
    void* d3d11_ctx_ = nullptr;
    uint64_t frame_id_ = 0;
    std::atomic<bool> request_keyframe_{false};
    bool first_frame_ = false;
};

} // namespace lt