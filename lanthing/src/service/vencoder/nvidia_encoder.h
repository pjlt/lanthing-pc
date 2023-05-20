#pragma once

#include <atomic>
#include <map>
#include <memory>

#include <ltrtc/lttypes.h>

#include "video_encoder.h"

namespace lt
{

namespace svc
{

class NvD3d11EncoderImpl;
class NvD3d11Encoder : public VideoEncoder
{
public:
    NvD3d11Encoder();
    ~NvD3d11Encoder() override;
    bool init(const InitParams& params);
    void reconfigure(const ReconfigureParams& params) override;
    EncodedFrame encode_one_frame(void* input_frame, bool request_iframe) override;

private:
    std::shared_ptr<NvD3d11EncoderImpl> impl_;
};

} // namespace svc

} // namespace lt
