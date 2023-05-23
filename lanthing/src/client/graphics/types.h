#pragma once

#include <cstdint>
#include <vector>

#include <string>

namespace lt
{

enum class Codec : uint8_t {
    UNKNOWN = 0,
    VIDEO_H264 = 1,
    VIDEO_H265 = 2,
};

constexpr inline Codec operator|(Codec a, Codec b) {
    return static_cast<Codec>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}

constexpr inline bool operator&(Codec a, Codec b) {
    return static_cast<uint8_t>(a) & static_cast<uint8_t>(b);
}

constexpr inline Codec& operator|=(Codec& a, Codec b) {
    return a = a | b;
}

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

constexpr inline bool operator&(Format a, Format b) {
    return static_cast<uint8_t>(a) & static_cast<uint8_t>(b);
}

constexpr inline Format operator|(Format a, Format b) {
    return static_cast<Format>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}

constexpr inline Format& operator|=(Format& a, Format b) {
    return a = a | b;
}

enum class GraphicsType : uint8_t {
    UNKNOWN = 0,
    DXGI = 1,
};

struct VideoFrame {
    int64_t no = 0;
    int64_t time = 0;

    GraphicsType type = GraphicsType::UNKNOWN;
};

enum class FrameType : uint8_t {
    UNKNOWN = 0,
    KEY_FRAME = 1,
    DELTA_FRAME_B = 2,
    DELTA_FRAME_P = 3,
};

struct EncodedFrame {
    int64_t no = 0;
    FrameType type = FrameType::UNKNOWN;

    std::vector<uint8_t> data;
};

} // namespace lt
