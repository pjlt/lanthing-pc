#pragma once
#include <memory>
#include <vector>

namespace lt {

class VideoRenderer {
public:
    struct Params {
        void* window;
        uint64_t device;
        uint32_t video_width;
        uint32_t video_height;
        uint32_t align;
    };

public:
    static std::unique_ptr<VideoRenderer> create(const Params& params);
    virtual ~VideoRenderer() = default;
    virtual bool bindTextures(const std::vector<void*>& textures) = 0;
    virtual bool render(int64_t frame) = 0;
    virtual bool waitForPipeline(int64_t max_wait_ms) = 0;
    virtual void* hwDevice() = 0;
    virtual void* hwContext() = 0;
};

} // namespace lt