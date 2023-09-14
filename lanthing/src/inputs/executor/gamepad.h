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

#pragma once
#include <ViGEmClient.h>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>

namespace lt {

constexpr uint32_t XUSER_MAX_COUNT = 4;
class Gamepad {
public:
    static std::unique_ptr<Gamepad>
    create(std::function<void(uint32_t, uint16_t, uint16_t)> gamepad_response);
    Gamepad(std::function<void(uint32_t, uint16_t, uint16_t)> gamepad_response);
    bool plugin(uint32_t index);
    void plugout(uint32_t index);
    bool submit(uint32_t index, const XUSB_REPORT& report);

private:
    bool connect();
    using UCHAR = unsigned char;
    static void on_gamepad_response(PVIGEM_CLIENT client, PVIGEM_TARGET target, UCHAR large_motor,
                                    UCHAR small_motor, UCHAR led_number);

private:
    static std::map<PVIGEM_CLIENT, Gamepad*> map_s;
    std::function<void(uint32_t, uint16_t, uint16_t)> gamepad_response_;
    PVIGEM_CLIENT gamepad_driver_ = nullptr;
    PVIGEM_TARGET gamepad_target_[XUSER_MAX_COUNT] = {nullptr};
};

} // namespace lt