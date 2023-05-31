#pragma once
#include <cstdint>
#include <memory>
#include <map>
#include <functional>
#include <ViGEmClient.h>

namespace lt
{

namespace worker
{
constexpr uint32_t XUSER_MAX_COUNT = 4;
class Gamepad
{
public:
    static std::unique_ptr<Gamepad> create(std::function<void(uint32_t, uint16_t, uint16_t)> gamepad_response);
    Gamepad(std::function<void(uint32_t, uint16_t, uint16_t)> gamepad_response);
    bool plugin(uint32_t index);
    void plugout(uint32_t index);
    bool submit(uint32_t index, const XUSB_REPORT& report);

private:
    bool connect();
    using UCHAR = unsigned char;
    static void on_gamepad_response(PVIGEM_CLIENT client, PVIGEM_TARGET target, UCHAR large_motor, UCHAR small_motor, UCHAR led_number);

private:
    static std::map<PVIGEM_CLIENT, Gamepad*> map_s;
    std::function<void(uint32_t, uint16_t, uint16_t)> gamepad_response_;
    PVIGEM_CLIENT gamepad_driver_ = nullptr;
    PVIGEM_TARGET gamepad_target_[XUSER_MAX_COUNT] = { nullptr };
};

} // namespace worker

} // namespace lt