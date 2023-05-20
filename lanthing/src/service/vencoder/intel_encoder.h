#pragma once
#include <cstdint>
#include <memory>
#include <service/vencoder/video_encoder.h>

namespace lt
{

namespace svc
{

class IntelEncoderImpl;

class IntelEncoder : public VideoEncoder
{
public:
    IntelEncoder();
    ~IntelEncoder() override { }
    bool init(const InitParams& params);
    void reconfigure(const ReconfigureParams& params) override;
    EncodedFrame encode_one_frame(void* input_frame, bool force_idr) override;

private:
    std::shared_ptr<IntelEncoderImpl> impl_;
};

} // namespace svc

} // namespace lt