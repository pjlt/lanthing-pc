/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2023 Zhennan Tu <zhennan.tu@gmail.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once
#include <array>
#include <cstdint>
#include <map>
#include <optional>
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
    std::array<uint32_t, 3> qmin_ = {10, 10, 25};
    std::array<uint32_t, 3> qmax_ = {40, 40, 42};
    std::optional<int> vbvbufsize_;
    std::optional<int> vbvinit_;
    std::map<std::string, std::string> params_;
};

} // namespace lt