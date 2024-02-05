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

#include "openh264_decoder.h"

#include <wels/codec_api.h>

#include <rtc/rtc.h>

#include <ltlib/load_library.h>
#include <ltlib/logging.h>

namespace lt {

namespace video {

struct OpenH264DecoderContext {
    ISVCDecoder* decoder = nullptr;
    decltype(&WelsCreateDecoder) create_decoder = nullptr;
    decltype(&WelsDestroyDecoder) destroy_decoder = nullptr;
};

OpenH264Decoder::OpenH264Decoder(const Params& params)
    : Decoder{params}
    , ctx_{std::make_shared<OpenH264DecoderContext>()} {}

OpenH264Decoder::~OpenH264Decoder() {
    if (ctx_->decoder != nullptr) {
        if (openh264_init_success_) {
            ctx_->decoder->Uninitialize();
        }
        ctx_->destroy_decoder(ctx_->decoder);
    }
}

bool OpenH264Decoder::init() {
    if (codecType() != VideoCodecType::H264_420) {
        LOG(ERR) << "OpenH264 decoder only support H264_420";
        return false;
    }
    if (!loadApi()) {
        return false;
    }
    int ret = ctx_->create_decoder(&ctx_->decoder);
    if (ret != 0) {
        LOG(ERR) << "WelsCreateDecoder failed with " << ret;
        return false;
    }
    SDecodingParam init_params{};
    init_params.eEcActiveIdc = ERROR_CON_DISABLE;
    init_params.sVideoProperty.eVideoBsType = VIDEO_BITSTREAM_DEFAULT;
    ret = ctx_->decoder->Initialize(&init_params);
    if (ret != 0) {
        LOG(ERR) << "ISVCDecoder::Initialize failed with " << ret;
        return false;
    }
    openh264_init_success_ = true;
    auto size = width() * height() * 3 / 2;
    frame_.resize(size);
    return true;
}

DecodedFrame OpenH264Decoder::decode(const uint8_t* data, uint32_t size) {
    DecodedFrame frame{};
    SBufferInfo info{};
    uint8_t* outputs[4] = {0};
    DECODING_STATE state = ctx_->decoder->DecodeFrame2(data, size, outputs, &info);
    if (state != DECODING_STATE::dsErrorFree) {
        LOG(ERR) << "ISVCDecoder::DecodeFrame2 failed with " << (int)state;
        frame.status = DecodeStatus::Failed;
        return frame;
    }
    if (info.iBufferStatus != 1) {
        LOG(ERR) << "ISVCDecoder::DecodeFrame2 ret iBufferStatus with " << info.iBufferStatus;
        frame.status = DecodeStatus::Failed;
        return frame;
    }
    auto w = static_cast<int>(width());
    auto h = static_cast<int>(height());
    int ret = rtc::I420ToNV12(outputs[0], info.UsrData.sSystemBuffer.iStride[0], outputs[1],
                              info.UsrData.sSystemBuffer.iStride[1], outputs[2],
                              info.UsrData.sSystemBuffer.iStride[1], frame_.data(), w,
                              frame_.data() + w * h, w, w, h);
    if (ret != 0) {
        LOG(ERR) << "rtc::I420ToNV12 failed " << ret;
        frame.status = DecodeStatus::Failed;
        return frame;
    }
    frame.frame = static_cast<int64_t>((uintptr_t)frame_.data());
    frame.status = DecodeStatus::Success2;
    return frame;
}

std::vector<void*> OpenH264Decoder::textures() {
    std::vector<void*> result;
    result.push_back(frame_.data());
    return result;
}

DecodedFormat OpenH264Decoder::decodedFormat() const {
    return DecodedFormat::MEM_NV12;
}

bool OpenH264Decoder::loadApi() {
    const std::string kLibName = "openh264-2.4.1-win64.dll";
    openh264_lib_ = ltlib::DynamicLibrary::load(kLibName);
    if (openh264_lib_ == nullptr) {
        LOG(ERR) << "Load library " << kLibName << " failed";
        return false;
    }
    ctx_->create_decoder =
        reinterpret_cast<decltype(&WelsCreateDecoder)>(openh264_lib_->getFunc("WelsCreateDecoder"));
    if (ctx_->create_decoder == nullptr) {
        LOG(ERR) << "Load function WelsCreateDecoder from " << kLibName << " failed";
        return false;
    }
    ctx_->destroy_decoder = reinterpret_cast<decltype(&WelsDestroyDecoder)>(
        openh264_lib_->getFunc("WelsDestroyDecoder"));
    if (ctx_->destroy_decoder == nullptr) {
        LOG(ERR) << "Load function WelsDestroyDecoder from " << kLibName << " failed";
        return false;
    }
    return true;
}

} // namespace video
} // namespace lt