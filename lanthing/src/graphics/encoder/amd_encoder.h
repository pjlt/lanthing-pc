#pragma once
#include <cstdint>
#include <memory>

#include "params_helper.h"
#include <graphics/encoder/video_encoder.h>

namespace lt {

class AmdEncoderImpl;

class AmdEncoder : public VideoEncoder {
public:
    AmdEncoder(void* d3d11_dev, void* d3d11_ctx);
    ~AmdEncoder() override {}
    bool init(const VideoEncodeParamsHelper& params);
    void reconfigure(const ReconfigureParams& params) override;
    EncodedFrame encode_one_frame(void* input_frame, bool force_idr) override;

private:
    std::shared_ptr<AmdEncoderImpl> impl_;
};

} // namespace lt