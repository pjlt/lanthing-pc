#pragma once

#include <cstdint>

#include <string>
#include <vector>

namespace lt {

class UiCallback {
public:
    enum class ErrCode : uint8_t {
        OK = 0,
        TIMEOUT,
        FALIED,
        SYSTEM_ERROR,
    };

public:
    virtual ~UiCallback() = default;

    virtual void onLoginRet(ErrCode code, const std::string& err = {}) = 0;

    virtual void onInviteRet(ErrCode code, const std::string& err = {}) = 0;

    virtual void onDisconnectedWithServer() = 0;

    virtual void onDevicesChanged(const std::vector<std::string>& dev_ids) = 0;
};
} // namespace lt
