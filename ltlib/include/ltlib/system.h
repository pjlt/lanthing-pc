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
#include <ltlib/ltlib.h>

#include <string>
#include <vector>

namespace ltlib {

std::string LT_API getProgramFullpath();

std::string LT_API getProgramPath();

std::string LT_API getConfigPath(bool is_service = false);

bool LT_API isRunasLocalSystem();
bool LT_API isRunAsService();

int32_t LT_API getScreenWidth();
int32_t LT_API getScreenHeight();

struct LT_API DisplayOutputDesc {
    DisplayOutputDesc() = delete;
    DisplayOutputDesc(uint32_t w, uint32_t h, uint32_t f, uint32_t r)
        : width(w)
        , height(h)
        , frequency(f)
        , rotation(r) {}
    int32_t width;
    int32_t height;
    int32_t frequency;
    int32_t rotation;
};
DisplayOutputDesc LT_API getDisplayOutputDesc();

bool LT_API changeDisplaySettings(uint32_t w, uint32_t h, uint32_t f);

bool LT_API setThreadDesktop();

bool LT_API selfElevateAndNeedExit();

struct LT_API Monitor {
    int32_t left;
    int32_t top;
    int32_t right;
    int32_t bottom;
    int32_t rotation;
};

std::vector<Monitor> LT_API enumMonitors();

} // namespace ltlib