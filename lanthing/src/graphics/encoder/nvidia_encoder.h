#pragma once

#include <atomic>
#include <map>
#include <memory>

#include <transport/transport.h>

#include "params_helper.h"
#include <graphics/encoder/video_encoder.h>

namespace lt {

class NvD3d11EncoderImpl;
class NvD3d11Encoder : public VideoEncoder {
public:
    NvD3d11Encoder(void* d3d11_dev, void* d3d11_ctx);
    ~NvD3d11Encoder() override;

    bool init(const VideoEncodeParamsHelper& params);
    void reconfigure(const ReconfigureParams& params) override;
    EncodedFrame encodeFrame(void* input_frame) override;

private:
    std::shared_ptr<NvD3d11EncoderImpl> impl_;
};

} // namespace lt