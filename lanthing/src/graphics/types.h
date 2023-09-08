#pragma once

#include <cstdint>
#include <vector>

#include <string>

namespace lt {

enum class Format : uint8_t {
    UNSUPPORT = 0,
    H264_NV12 = 1,
    H265_NV12 = 2,
    H264_YUV444 = 4,
    H265_YUV444 = 8,
};

inline std::string videoFormatToString(Format format) {
    switch (format) {
    case Format::H264_NV12:
        return "h264-nv12";
    case Format::H264_YUV444:
        return "h264-yuv444";
    case Format::H265_NV12:
        return "h265-nv12";
    case Format::H265_YUV444:
        return "h265-yuv444";
    default:
        return "unknown format";
    }
}

enum class VaType {
    D3D11,
};

} // namespace lt
