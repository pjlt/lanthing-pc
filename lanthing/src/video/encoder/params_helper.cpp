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

#include "params_helper.h"

#include <cassert>
#include <sstream>

namespace lt {

namespace video {

EncodeParamsHelper::EncodeParamsHelper(void* d3d11_dev, void* d3d11_ctx, int64_t luid,
                                       lt::VideoCodecType c, uint32_t width, uint32_t height,
                                       uint32_t fps, uint32_t bitrate, bool enable_vbv)
    : d3d11_dev_{d3d11_dev}
    , d3d11_ctx_{d3d11_ctx}
    , luid_{luid}
    , codec_type_{c}
    , width_{width}
    , height_{height}
    , fps_{static_cast<int>(fps)} // 为什么用int？忘了
    , bitrate_{bitrate}
    , enable_vbv_{enable_vbv}
    , profile_{codecToProfile(c)} {
    calc_vbv();
    std::stringstream ssQmin;
    std::stringstream ssQmax;
    ssQmin << qmin_[0] << ',' << qmin_[1] << ',' << qmin_[2];
    ssQmax << qmax_[0] << ',' << qmax_[1] << ',' << qmax_[2];
    params_["-width"] = std::to_string(width_);
    params_["-height"] = std::to_string(width_);
    params_["-fps"] = std::to_string(fps);
    params_["-bitrate"] = std::to_string(bitrate_);
    params_["-maxbitrate"] = std::to_string(maxbitrate());
    params_["-gop"] = std::to_string(gop_);
    params_["-rc"] = std::to_string((int)rc_);
    params_["-preset"] = std::to_string((int)preset_);
    params_["-profile"] = std::to_string((int)profile_);
    params_["-qmin"] = ssQmin.str();
    params_["-qmax"] = ssQmax.str();
    switch (codec_type_) {
    case VideoCodecType::H264_420:
    case VideoCodecType::H264_444:
        params_["-codec"] = "h264";
        break;
    case VideoCodecType::H265_420:
    case VideoCodecType::H265_444:
        params_["-codec"] = "hevc";
        break;
    default:
        params_["-codec"] = "unknown";
        break;
    }
}

void EncodeParamsHelper::set_bitrate(uint32_t bps) {
    bitrate_ = bps;
    calc_vbv();
}

void EncodeParamsHelper::set_fps(int _fps) {
    fps_ = _fps;
    calc_vbv();
}

std::string EncodeParamsHelper::params() const {
    std::stringstream oss;
    for (const auto& param : params_) {
        if (param.first.empty() || param.second.empty()) {
            continue;
        }
        oss << param.first << " " << param.second << " ";
    }
    return oss.str();
}

EncodeParamsHelper::Profile EncodeParamsHelper::codecToProfile(lt::VideoCodecType codec) {
    switch (codec) {
    case lt::VideoCodecType::H264_420:
        return Profile::AvcMain;
    case lt::VideoCodecType::H265_420:
        return Profile::HevcMain;
    default:
        return Profile::AvcMain;
    }
}

void EncodeParamsHelper::calc_vbv() {
    if (enable_vbv_) {
        float vbv = 1.3f;
        int bitrate_vbv = static_cast<int>(bitrate_ * vbv + 0.5f);
        int vbv_buf = static_cast<int>(bitrate_vbv * 1.0f / fps_ + 0.5f);
        vbvbufsize_ = vbv_buf;
        vbvinit_ = vbv_buf;
        params_["-vbvbufsize"] = std::to_string(vbv_buf);
        params_["-vbvinit"] = std::to_string(vbv_buf);
    }
}

} // namespace video

} // namespace lt