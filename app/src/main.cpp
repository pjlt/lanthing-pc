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

#include "app.h"

#include <cstdarg>
#include <filesystem>
#include <memory>
#include <regex>

#include <g3log/g3log.hpp>
#include <g3log/logworker.hpp>
#include <lt_minidump_generator.h>

#include <ltlib/log_sink.h>
#include <ltlib/system.h>
#include <ltlib/threads.h>

#if defined(LT_WINDOWS) && LT_RUN_AS_SERVICE
// 不显示命令行窗口.
#pragma comment(linker, "/subsystem:\"windows\" /entry:\"mainCRTStartup\"")
#endif

std::unique_ptr<g3::LogWorker> g_logWorker;
std::unique_ptr<g3::FileSinkHandle> g_logsSink;
std::unique_ptr<LTMinidumpGenerator> g_minidumpGenertator;

namespace {

void sigint_handler(int) {
    LOG(INFO) << "SIGINT Received";
    g_logWorker.reset();
    g_logsSink.reset();
    g_minidumpGenertator.reset();
    std::terminate();
}

void initLogging() {
    std::string bin_path = ltlib::getProgramFullpath<char>();
    std::string bin_dir = ltlib::getProgramPath<char>();
    std::string appdata_dir = ltlib::getAppdataPath(/*is_win_service=*/false);
    std::string kPrefix = "ui";
    std::filesystem::path log_dir;
    if (!appdata_dir.empty()) {
        log_dir = appdata_dir;
        log_dir /= "lanthing";
        log_dir /= kPrefix;
    }
    else {
        log_dir = bin_dir;
        log_dir /= "log";
    }
    if (!std::filesystem::exists(log_dir)) {
        if (!std::filesystem::create_directories(log_dir)) {
            ::printf("Create log directory '%s' failed\n", log_dir.string().c_str());
        }
    }
    g_logWorker = g3::LogWorker::createLogWorker();
    g_logWorker->addSink(std::make_unique<ltlib::LogSink>(kPrefix, log_dir.string()),
                         &ltlib::LogSink::fileWrite);
    g3::initializeLogging(g_logWorker.get());
    ltlib::ThreadWatcher::instance()->registerTerminateCallback(
        [](const std::string& last_word) { LOG(INFO) << "Last words: " << last_word; });

    LOG(INFO) << "Log system initialized";

    g_minidumpGenertator = std::make_unique<LTMinidumpGenerator>(log_dir.string());
    signal(SIGINT, sigint_handler);
}

} // namespace

int main(int argc, char** argv) {
    ::srand(time(nullptr)); // 为什么这个srand不生效?
    initLogging();
    std::unique_ptr<lt::App> app = lt::App::create();
    if (app == nullptr) {
        return -1;
    }
    LOG(INFO) << "app run.";
    return app->exec(argc, argv);
}
