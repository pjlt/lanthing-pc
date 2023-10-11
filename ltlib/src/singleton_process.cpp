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

#ifdef LT_WINDOWS
#include <Windows.h>
#elif defined LT_LINUX
#include <errno.h>
#include <sys/file.h>
#endif

#include <filesystem>
#include <sstream>
#include <string>

#include <ltlib/event.h>
#include <ltlib/singleton_process.h>
#include <ltlib/strings.h>
#include <ltlib/system.h>

namespace ltlib {

#ifdef LT_WINDOWS

bool makeSingletonProcess(const std::string& name) {
    static std::optional<bool> no_other_process;
    if (no_other_process.has_value()) {
        return no_other_process.value();
    }
    std::string event_name = "Global\\singleton_process_" + name;
    std::wstring wname = ltlib::utf8To16(event_name);
    // no delete
    auto singleton = new ltlib::Event{event_name};
    no_other_process = singleton->isOwner();
    return no_other_process.value();
}

#elif defined LT_LINUX
bool makeSingletonProcess(const std::string& name) {
    static std::optional<bool> no_other_process;
    if (no_other_process.has_value()) {
        return no_other_process.value();
    }
    std::stringstream ss;
    ss << "/var/run/" << name << ".pid";
    int pid_file = open(ss.str().c_str(), O_CREAT | O_RDWR, 0666);
    if (lockf(pidfile, F_TLOCK, 0) < 0) {
        no_other_process = false;
        return false;
    }
    char str[64] = {0};
    snprintf(str, 64, "%d", getpid());
    write(pid_file, str, strlen(str));
}

#else
#error unsupported platform
#endif

} // namespace ltlib