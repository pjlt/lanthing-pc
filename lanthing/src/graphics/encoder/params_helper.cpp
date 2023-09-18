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
        float vbv = 2.6f;
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