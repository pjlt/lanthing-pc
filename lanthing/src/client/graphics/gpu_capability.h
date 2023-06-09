#pragma once

#include <string>
#include <vector>

#include "types.h"

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
        std::vector<Format> formats;

        std::string to_str() const;
    };

public:
    bool init();

    std::vector<Ability>& get() { return abilities_; }

private:
    std::vector<Ability> abilities_;
};
}; // namespace lt
