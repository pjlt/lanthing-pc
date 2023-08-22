#pragma once
#include <array>
#include <cstdint>
#include <map>
#include <string>

#include <transport/transport.h>

namespace lt {

class VideoEncodeParamsHelper {
public:
    enum class RcMode {
        Unknown,
        VBR,
        CBR,
    };
    enum class Preset {
        Unknown,
        Balanced,
        Speed,
        Quality,
    };
    enum class Profile {
        Unknown,
        AvcMain,
        HevcMain,
    };

public:
    VideoEncodeParamsHelper(lt::VideoCodecType c, uint32_t width, uint32_t height, int fps,
                            uint32_t bitrate_kbps, bool enable_vbv);

    uint32_t width() const { return width_; }
    uint32_t height() const { return height_; }
    int fps() const { return fps_; }
    uint32_t bitrate() const { return bitrate_kbps_ * 1024; }
    uint32_t bitrate_kbps() const { return bitrate_kbps_; }
    uint32_t maxbitrate_kbps() const { return static_cast<uint32_t>(bitrate_kbps_ * 1.05f); }
    uint32_t maxbitrate() const { return static_cast<uint32_t>(bitrate_kbps_ * 1024 * 1.05f); }
    std::array<uint32_t, 3> qmin() const { return qmin_; }
    std::array<uint32_t, 3> qmax() const { return qmax_; }
    std::optional<int> vbvbufsize() const { return vbvbufsize_; }
    std::optional<int> vbvinit() const { return vbvinit_; }
    int gop() const { return gop_; }
    RcMode rc() const { return rc_; }
    Preset preset() const { return preset_; }
    lt::VideoCodecType codec() const { return codec_type_; }
    Profile profile() const { return profile_; }

    std::string params() const;

private:
    const lt::VideoCodecType codec_type_;
    const uint32_t width_;
    const uint32_t height_;
    const int fps_;
    const uint32_t bitrate_kbps_;
    const bool enable_vbv_;
    const int gop_ = -1;
    const RcMode rc_ = RcMode::VBR;
    const Preset preset_ = Preset::Speed;
    const Profile profile_;
    std::array<uint32_t, 3> qmin_ = {10, 10, 10};
    std::array<uint32_t, 3> qmax_ = {40, 40, 40};
    std::optional<int> vbvbufsize_;
    std::optional<int> vbvinit_;
    std::map<std::string, std::string> params_;
};

} // namespace lt