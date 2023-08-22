#pragma once
#include <cstdint>
#include <memory>

#include "params_helper.h"
#include <graphics/encoder/video_encoder.h>

namespace lt {

class IntelEncoderImpl;

class IntelEncoder : public VideoEncoder {
public:
    IntelEncoder(void* d3d11_dev, void* d3d11_ctx, int64_t luid);
    ~IntelEncoder() override {}
    bool init(const VideoEncodeParamsHelper& params);
    void reconfigure(const ReconfigureParams& params) override;
    EncodedFrame encode_one_frame(void* input_frame, bool force_idr) override;

private:
    std::shared_ptr<IntelEncoderImpl> impl_;
};

} // namespace lt