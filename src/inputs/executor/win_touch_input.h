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
#include <Windows.h>

#include <cstdint>

#include <array>
#include <memory>
#include <optional>

#include <google/protobuf/message_lite.h>

#include <ltlib/load_library.h>
#include <ltlib/system.h>

namespace lt {

namespace input {

class WinTouch {
public:
    static std::unique_ptr<WinTouch> create(uint32_t screen_width, uint32_t screen_height,
                                            ltlib::Monitor monitor);
    ~WinTouch();

    bool submit(const std::shared_ptr<google::protobuf::MessageLite>& msg);
    void update();

private:
    WinTouch(uint32_t screen_width, uint32_t screen_height, ltlib::Monitor monitor);
    bool init();
    bool init2();
    bool reset();
    void resetPointState();
    bool injectSyntheticPointerInput(HSYNTHETICPOINTERDEVICE device,
                                     const std::vector<POINTER_TYPE_INFO>& pointerInfo,
                                     uint32_t count);

private:
    uint32_t screen_width_;
    uint32_t screen_height_;
    ltlib::Monitor monitor_;
    std::optional<bool> init_success_;
    std::vector<POINTER_TYPE_INFO> points_;
    uint32_t using_points_ = 0;
    HSYNTHETICPOINTERDEVICE touch_dev_ = nullptr;
    int32_t offset_x_ = 0;
    int32_t offset_y_ = 0;
    decltype(InjectSyntheticPointerInput)* inject_pointer_ = nullptr;
    decltype(DestroySyntheticPointerDevice)* destroy_pointer_ = nullptr;
    decltype(CreateSyntheticPointerDevice)* create_pointer_ = nullptr;
    std::unique_ptr<ltlib::DynamicLibrary> user32_lib_;
};

} // namespace input

} // namespace lt