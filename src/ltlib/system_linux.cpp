/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2026 Zhennan Tu <zhennan.tu@gmail.com>
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

#include <X11/Xlib.h>
#include <pwd.h>
#include <unistd.h>

#include <climits>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

#include <ltlib/system.h>

namespace ltlib {

std::string getProgramFullpath() {
    char result[PATH_MAX];
    auto count = readlink("/proc/self/exe", result, PATH_MAX);
    if (count <= 0) {
        return "";
    }
    return std::string(result, result + count);
}

std::string getProgramPath() {
    std::string fullpath = getProgramFullpath();
    if (fullpath.empty()) {
        return "";
    }
    auto pos = fullpath.rfind('/');
    if (pos != std::string::npos) {
        return fullpath.substr(0, pos);
    }
    return "";
}

std::string getProgramName() {
    std::string fullpath = getProgramFullpath();
    if (fullpath.empty()) {
        return "";
    }
    auto pos = fullpath.rfind('/');
    if (pos != std::string::npos) {
        return fullpath.substr(pos + 1);
    }
    return "";
}

std::string getConfigPath(bool is_service) {
    (void)is_service;
    static std::string config_path;
    if (!config_path.empty()) {
        return config_path;
    }
    std::filesystem::path fs = getpwuid(getuid())->pw_dir;
    fs = fs / ".lanthing";
    config_path = fs.string();
    return config_path;
}

bool isRunasLocalSystem() {
    return false;
}

bool isRunAsService() {
    return false;
}

int32_t getScreenWidth() {
    return -1;
}

int32_t getScreenHeight() {
    return -1;
}

DisplayOutputDesc getDisplayOutputDesc(const std::string& name) {
    (void)name;
    Display* d = XOpenDisplay(nullptr);
    if (d == nullptr) {
        return {0, 0, 0, 0};
    }
    Screen* s = DefaultScreenOfDisplay(d);
    if (s == nullptr) {
        return {0, 0, 0, 0};
    }
    uint32_t width = s->width;
    uint32_t height = s->height;
    XCloseDisplay(d);
    return {width, height, 60, 0};
}

bool changeDisplaySettings(uint32_t w, uint32_t h, uint32_t f) {
    (void)w;
    (void)h;
    (void)f;
    return false;
}

bool setThreadDesktop() {
    return false;
}

bool selfElevateAndNeedExit() {
    return false;
}

std::vector<Monitor> enumMonitors() {
    return {};
}

void openFolder(const std::string& path) {
    (void)path;
}

void putenv(const std::string& key, const std::string& value) {
    ::setenv(key.c_str(), value.c_str(), 1);
}

} // namespace ltlib
