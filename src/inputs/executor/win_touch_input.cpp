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

#include "win_touch_input.h"

#include <ltproto/client2worker/touch_event.pb.h>

#include <ltlib/logging.h>
#include <ltlib/system.h>

namespace {

constexpr size_t kMaxPoints = 10;

uint32_t reArrangePoints(std::vector<POINTER_TYPE_INFO>& points) {
    size_t i;
    for (i = 0; i < points.size(); i++) {
        if (points[i].touchInfo.pointerInfo.pointerFlags == POINTER_FLAG_NONE) {
            for (auto j = i + 1; j < points.size(); j++) {
                if (points[j].touchInfo.pointerInfo.pointerFlags != POINTER_FLAG_NONE) {
                    std::swap(points[i], points[j]);
                    break;
                }
            }
            if (points[i].touchInfo.pointerInfo.pointerFlags == POINTER_FLAG_NONE) {
                break;
            }
        }
    }
    return static_cast<uint32_t>(i);
}

void convert(const std::shared_ptr<ltproto::client2worker::TouchEvent>& msg, POINTER_INFO& point,
             const ltlib::Monitor& monitor, int32_t w, int32_t h, int32_t ox, int32_t oy) {
    using namespace ltproto::client2worker;
    switch (msg->touch_flag()) {
    case TouchEvent_TouchFlag_TouchUp:
        point.pointerFlags &= ~(POINTER_FLAG_INCONTACT | POINTER_FLAG_INRANGE);
        point.pointerFlags |= POINTER_FLAG_UP;
        break;
    case TouchEvent_TouchFlag_TouchDown:
        point.pointerFlags |= POINTER_FLAG_INRANGE | POINTER_FLAG_INCONTACT | POINTER_FLAG_DOWN;
        point.ptPixelLocation.x = static_cast<LONG>(msg->x() * w + monitor.left - ox);
        point.ptPixelLocation.y = static_cast<LONG>(msg->y() * h + monitor.top - oy);
        break;
    case TouchEvent_TouchFlag_TouchMove:
        point.pointerFlags |= POINTER_FLAG_INRANGE | POINTER_FLAG_INCONTACT | POINTER_FLAG_UPDATE;
        point.ptPixelLocation.x = static_cast<LONG>(msg->x() * w + monitor.left - ox);
        point.ptPixelLocation.y = static_cast<LONG>(msg->y() * h + monitor.top - oy);
        break;
    case TouchEvent_TouchFlag_TouchCancel:
        if (point.pointerFlags & POINTER_FLAG_INCONTACT) {
            point.pointerFlags |= POINTER_FLAG_UP;
        }
        else {
            point.pointerFlags |= POINTER_FLAG_UPDATE;
        }
        point.pointerFlags &= ~(POINTER_FLAG_INCONTACT | POINTER_FLAG_INRANGE);
        point.pointerFlags |= POINTER_FLAG_CANCELED;
        break;
    default:
        LOG(WARNING) << "Unknown TouchFlag " << (int)msg->touch_flag() << " "
                     << ltproto::client2worker::TouchEvent_TouchFlag_Name(msg->touch_flag());
        break;
    }
}

} // namespace

namespace lt {

namespace input {

std::unique_ptr<WinTouch> WinTouch::create(uint32_t screen_width, uint32_t screen_height,
                                           ltlib::Monitor monitor) {
    std::unique_ptr<WinTouch> touch{new WinTouch{screen_width, screen_height, monitor}};
    if (!touch->init()) {
        return nullptr;
    }
    return touch;
}

WinTouch::~WinTouch() {
    if (touch_dev_ != nullptr) {
        destroy_pointer_(touch_dev_);
    }
}

WinTouch::WinTouch(uint32_t screen_width, uint32_t screen_height, ltlib::Monitor monitor)
    : screen_width_{screen_width}
    , screen_height_{screen_height}
    , monitor_{monitor} {
    resetPointState();
}

bool WinTouch::init() {
    offset_x_ = GetSystemMetrics(SM_XVIRTUALSCREEN);
    offset_y_ = GetSystemMetrics(SM_YVIRTUALSCREEN);
    const std::string kLibName = "user32.dll";
    user32_lib_ = ltlib::DynamicLibrary::load(kLibName);
    if (user32_lib_ == nullptr) {
        LOG(ERR) << "Load library " << kLibName << " failed";
        return false;
    }
    create_pointer_ = reinterpret_cast<decltype(&CreateSyntheticPointerDevice)>(
        user32_lib_->getFunc("CreateSyntheticPointerDevice"));
    if (create_pointer_ == nullptr) {
        LOG(ERR) << "Load function CreateSyntheticPointerDevice from " << kLibName << " failed";
        return false;
    }
    destroy_pointer_ = reinterpret_cast<decltype(&DestroySyntheticPointerDevice)>(
        user32_lib_->getFunc("DestroySyntheticPointerDevice"));
    if (destroy_pointer_ == nullptr) {
        LOG(ERR) << "Load function DestroySyntheticPointerDevice from " << kLibName << " failed";
        return false;
    }
    inject_pointer_ = reinterpret_cast<decltype(&InjectSyntheticPointerInput)>(
        user32_lib_->getFunc("InjectSyntheticPointerInput"));
    if (inject_pointer_ == nullptr) {
        LOG(ERR) << "Load function InjectSyntheticPointerInput from " << kLibName << " failed";
        return false;
    }
    return true;
}

bool WinTouch::init2() {
    if (init_success_.has_value()) {
        return init_success_.value();
    }
    touch_dev_ =
        create_pointer_(PT_TOUCH, static_cast<ULONG>(points_.size()), POINTER_FEEDBACK_DEFAULT);
    if (touch_dev_ == nullptr) {
        init_success_ = false;
        LOGF(ERR, "CreateSyntheticPointerDevice failed with %#x", GetLastError());
        return false;
    }
    LOG(INFO) << "CreateSyntheticPointerDevice success";
    init_success_ = true;
    return true;
}

bool WinTouch::reset() {
    bool success = true;
    using_points_ = reArrangePoints(points_);
    if (using_points_ != 0) {
        for (uint32_t i = 0; i < using_points_; i++) {
            if (points_[i].touchInfo.pointerInfo.pointerFlags & POINTER_FLAG_INCONTACT) {
                points_[i].touchInfo.pointerInfo.pointerFlags |= POINTER_FLAG_UP;
            }
            else {
                points_[i].touchInfo.pointerInfo.pointerFlags |= POINTER_FLAG_UPDATE;
            }
            points_[i].touchInfo.pointerInfo.pointerFlags &=
                ~(POINTER_FLAG_INCONTACT | POINTER_FLAG_INRANGE);
            points_[i].touchInfo.pointerInfo.pointerFlags |= POINTER_FLAG_CANCELED;
            points_[i].touchInfo.touchMask = TOUCH_MASK_NONE;
        }
        success = injectSyntheticPointerInput(touch_dev_, points_, using_points_);
    }
    resetPointState();
    using_points_ = 0;
    return success;
}

void WinTouch::resetPointState() {
    points_.resize(kMaxPoints);
    for (auto& point : points_) {
        point.type = PT_TOUCH;
        point.touchInfo.pointerInfo.pointerType = PT_TOUCH;
    }
}

bool WinTouch::submit(const std::shared_ptr<google::protobuf::MessageLite>& _msg) {
    auto msg = std::static_pointer_cast<ltproto::client2worker::TouchEvent>(_msg);
    if (!init2()) {
        return false;
    }
    if (msg->touch_flag() == ltproto::client2worker::TouchEvent_TouchFlag_TouchCancel) {
        return reset();
    }
    size_t i = 0;
    for (i = 0; i < points_.size(); i++) {
        if (points_[i].touchInfo.pointerInfo.pointerId == msg->pointer_id() &&
            points_[i].touchInfo.pointerInfo.pointerFlags != POINTER_FLAG_NONE) {
            break;
        }
    }
    if (i >= points_.size()) {
        for (i = 0; i < points_.size(); i++) {
            if (points_[i].touchInfo.pointerInfo.pointerFlags == POINTER_FLAG_NONE) {
                points_[i].touchInfo.pointerInfo.pointerId = msg->pointer_id();
                using_points_ = static_cast<uint32_t>(i + 1);
                break;
            }
        }
    }
    if (i >= points_.size()) {
        LOG(WARNING) << "Too many touch points, up to " << points_.size() << " supported";
        return false;
    }
    convert(msg, points_[i].touchInfo.pointerInfo, monitor_, screen_width_, screen_height_,
            offset_x_, offset_y_);
    points_[i].touchInfo.touchMask = TOUCH_MASK_NONE;
    if (points_[i].touchInfo.pointerInfo.pointerFlags & POINTER_FLAG_INCONTACT) {
        if (msg->pressure() != 0) {
            points_[i].touchInfo.touchMask |= TOUCH_MASK_PRESSURE;
            points_[i].touchInfo.pressure = msg->pressure();
        }
        else {
            points_[i].touchInfo.pressure = 512;
        }
        if (msg->left() != 0 || msg->top() != 0 || msg->right() != 0 || msg->bottom() != 0) {
            points_[i].touchInfo.rcContact =
                RECT{msg->left(), msg->top(), msg->right(), msg->bottom()};
        }
        else {
            points_[i].touchInfo.rcContact =
                RECT{points_[i].touchInfo.pointerInfo.ptPixelLocation.x - 1,
                     points_[i].touchInfo.pointerInfo.ptPixelLocation.y - 1,
                     points_[i].touchInfo.pointerInfo.ptPixelLocation.x + 1,
                     points_[i].touchInfo.pointerInfo.ptPixelLocation.y + 1};
        }
        points_[i].touchInfo.touchMask |= TOUCH_MASK_CONTACTAREA;
    }
    else {
        points_[i].touchInfo.pressure = 0;
        points_[i].touchInfo.rcContact = {};
    }
    if (msg->orientation() < 360) {
        points_[i].touchInfo.touchMask |= TOUCH_MASK_ORIENTATION;
        points_[i].touchInfo.orientation = msg->orientation();
    }
    else {
        points_[i].touchInfo.orientation = 0;
    }
    bool success = injectSyntheticPointerInput(touch_dev_, points_, using_points_);
    points_[i].touchInfo.pointerInfo.pointerFlags &=
        ~(POINTER_FLAG_DOWN | POINTER_FLAG_UP | POINTER_FLAG_CANCELED | POINTER_FLAG_UPDATE);
    return success;
}

void WinTouch::update() {
    if (touch_dev_ == nullptr) {
        return;
    }
    injectSyntheticPointerInput(touch_dev_, points_, using_points_);
}

bool WinTouch::injectSyntheticPointerInput(HSYNTHETICPOINTERDEVICE device,
                                           const std::vector<POINTER_TYPE_INFO>& pointerInfo,
                                           uint32_t count) {
    BOOL ret = inject_pointer_(device, pointerInfo.data(), count);
    if (ret == TRUE) {
        return true;
    }
    if (!ltlib::setThreadDesktop()) {
        LOG(WARNING) << "WinTouch::submit SetThreadDesktop failed";
        return false;
    }
    ret = inject_pointer_(device, pointerInfo.data(), count);
    if (ret == FALSE) {
        LOGF(WARNING, "InjectSyntheticPointerInput failed with %#x", GetLastError());
        return false;
    }
    return true;
}

} // namespace input

} // namespace lt