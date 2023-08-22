#include "params_helper.h"

#include <cassert>
#include <sstream>

namespace lt {
VideoEncodeParamsHelper::VideoEncodeParamsHelper(lt::VideoCodecType c, uint32_t width,
                                                 uint32_t height, int fps, uint32_t bitrate_kbps,
                                                 bool enable_vbv)
    : codec_type_{c}
    , width_{width}
    , height_{height}
    , fps_{fps}
    , bitrate_kbps_{bitrate_kbps}
    , enable_vbv_{enable_vbv}
    , profile_{c == lt::VideoCodecType::H264 ? Profile::AvcMain : Profile::HevcMain} {
    assert(c == lt::VideoCodecType::H264 || c == lt::VideoCodecType::H265);
    uint32_t bitrate_bps = bitrate_kbps_ * 1024;
    if (enable_vbv) {
        float vbv = 0.f;
        if (bitrate_bps >= 12 * 1024 * 1024) {
            qmin_ = {14, 14, 24};
            qmax_ = {39, 39, 39};
            vbv = 2.7f;
        }
        else if (bitrate_bps >= 8 * 1024 * 1024) {
            qmin_ = {15, 15, 24};
            qmax_ = {40, 40, 41};
            vbv = 2.6f;
        }
        else if (bitrate_bps >= 6 * 1024 * 1024) {
            qmin_ = {17, 17, 25};
            qmax_ = {42, 42, 42};
            vbv = 2.4f;
        }
        else if (bitrate_bps >= 4 * 1024 * 1024) {
            qmin_ = {18, 18, 26};
            qmax_ = {43, 43, 42};
            vbv = 2.3f;
        }
        else if (bitrate_bps >= 3 * 1024 * 1024) {
            qmin_ = {19, 19, 27};
            qmax_ = {44, 44, 43};
            vbv = 2.1f;
        }
        else {
            qmin_ = {21, 21, 28};
            qmax_ = {47, 47, 46};
            vbv = 2.1f;
        }
        int bitrate_vbv = static_cast<int>(bitrate_bps * vbv + 0.5f);
        int vbv_buf = static_cast<int>(bitrate_vbv * 1.0f / fps_ + 0.5f);
        vbvbufsize_ = vbv_buf;
        vbvinit_ = vbv_buf;
        params_["-vbvbufsize"] = std::to_string(vbv_buf);
        params_["-vbvinit"] = std::to_string(vbv_buf);
    }
    std::stringstream ssQmin;
    std::stringstream ssQmax;
    ssQmin << qmin_[0] << ',' << qmin_[1] << ',' << qmin_[2];
    ssQmax << qmax_[0] << ',' << qmax_[1] << ',' << qmax_[2];
    params_["-bitrate"] = std::to_string(bitrate_bps);
    params_["-maxbitrate"] = std::to_string(bitrate_bps * 1.05f);
    params_["-codec"] = c == lt::VideoCodecType::H264 ? "h264" : "hevc";
    params_["-gop"] = std::to_string(gop_);
    params_["-rc"] = std::to_string((int)rc_);
    params_["-preset"] = std::to_string((int)preset_);
    params_["-profile"] = std::to_string((int)profile_);
    params_["-qmin"] = ssQmin.str();
    params_["-qmax"] = ssQmax.str();
    params_["-fps"] = std::to_string(fps);
}

std::string VideoEncodeParamsHelper::params() const {
    std::stringstream oss;
    for (const auto& param : params_) {
        if (param.first.empty() || param.second.empty()) {
            continue;
        }
        oss << param.first << " " << param.second << " ";
    }
    return oss.str();
}

} // namespace lt