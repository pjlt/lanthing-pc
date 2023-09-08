#pragma once

#include <string>
#include <vector>

#include <transport/transport.h>

namespace lt {
class GpuInfo {
public:
    struct Ability {
        uint64_t luid = 0;
        uint32_t vendor = 0;
        std::string desc;
        uint32_t device_id = 0;
        uint32_t video_memory_mb = 0;
        std::string driver;
        // 暂时写死NV12，不然一部分处理像素格式，一部分又不管，很混乱
        std::vector<lt::VideoCodecType> codecs;

        std::string to_str() const;
    };

public:
    bool init();

    std::vector<Ability>& get() { return abilities_; }

private:
    std::vector<Ability> abilities_;
};
}; // namespace lt
