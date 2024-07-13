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

#include "worker_check_decode.h"

#include <future>

#include <lt_constants.h>
#include <plat/video_device.h>
#include <video/drpipeline/video_decode_render_pipeline.h>

namespace lt {

namespace worker {

std::tuple<std::unique_ptr<WorkerCheckDecode>, int32_t>
WorkerCheckDecode::create(std::map<std::string, std::string> options) {
    (void)options;
    auto empty_func = []() {};
    lt::plat::PcSdl::Params sdl_params{};
    sdl_params.on_reset = empty_func;
    sdl_params.hide_window = true;
    auto sdl = lt::plat::PcSdl::create(sdl_params);
    if (sdl == nullptr) {
        return {nullptr, kExitCodeInitWorkerFailed};
    }
    uint32_t codecs = 0;
    for (auto codec :
         {VideoCodecType::H265_420, VideoCodecType::H264_420, VideoCodecType::H264_420_SOFT}) {
        auto empty_func2 = [](uint32_t, std::shared_ptr<google::protobuf::MessageLite>, bool) {};
        auto video_device = plat::VideoDevice::create(codec);
        if (video_device == nullptr) {
            continue;
        }
        lt::video::DecodeRenderPipeline::Params params{
            codec, codec, 1920, 1080, 60, 0, true, empty_func2, empty_func, empty_func};
        params.sdl = sdl.get();
        params.device = video_device->device();
        params.context = video_device->context();
        params.for_test = true;
        auto pipeline = lt::video::DecodeRenderPipeline::create(params);
        if (pipeline != nullptr) {
            codecs = codecs | codec;
        }
    }
    std::unique_ptr<WorkerCheckDecode> w{new WorkerCheckDecode{codecs}};
    return {std::move(w), kExitCodeOK};
}

WorkerCheckDecode::WorkerCheckDecode(uint32_t codecs)
    : codecs_{codecs} {}

WorkerCheckDecode::~WorkerCheckDecode() = default;

int WorkerCheckDecode::wait() {
    return static_cast<int>(codecs_);
}

} // namespace worker

} // namespace lt