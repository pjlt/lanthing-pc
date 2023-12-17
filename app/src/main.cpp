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

#include <g3log/logworker.hpp>
#include <lt_minidump_generator.h>

#include <ltlib/logging.h>
#include <ltlib/singleton_process.h>
#include <ltlib/strings.h>
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

void terminateCallback(const std::string& last_word) {
    LOG(INFO) << "Last words: " << last_word;
}

void cleanupDumps(const std::filesystem::path& path) {
    while (true) {
        auto now = std::filesystem::file_time_type::clock::now();
        for (const auto& file : std::filesystem::directory_iterator{path}) {
            std::string filename = file.path().string();
            if (filename.size() < 5 || filename.substr(filename.size() - 4) != ".dmp") {
                continue;
            }
            if (file.last_write_time().time_since_epoch() >
                (now - std::chrono::days{14}).time_since_epoch()) {
                continue;
            }
            std::filesystem::remove(file.path());
            LOG(INFO) << "Removing dump " << file.path().string();
        }
        std::this_thread::sleep_for(std::chrono::hours{12});
    }
}

void initLoggingAndDumps() {
    std::string bin_path = ltlib::getProgramFullpath();
    std::string bin_dir = ltlib::getProgramPath();
    std::string appdata_dir = ltlib::getConfigPath(/*is_win_service=*/false);
    std::wstring w_appdata_dir = ltlib::utf8To16(appdata_dir);
    std::string kPrefix = "app";
    std::filesystem::path log_dir;
    if (!appdata_dir.empty()) {
        log_dir = w_appdata_dir;
        log_dir /= "log";
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
    g3::only_change_at_initialization::addLogLevel(ERR);
    g3::log_levels::disable(DEBUG);
    g3::initializeLogging(g_logWorker.get());

    LOG(INFO) << "Log system initialized";

    std::thread cleanup_dumps([log_dir]() { cleanupDumps(log_dir); });
    cleanup_dumps.detach();

    g_minidumpGenertator = std::make_unique<LTMinidumpGenerator>(log_dir.string());
    signal(SIGINT, sigint_handler);
    if (LT_CRASH_ON_THREAD_HANGS) {
        ltlib::ThreadWatcher::instance()->enableCrashOnTimeout();
        ltlib::ThreadWatcher::instance()->registerTerminateCallback(terminateCallback);
    }
    else {
        ltlib::ThreadWatcher::instance()->disableCrashOnTimeout();
    }
}

} // namespace

int main(int argc, char** argv) {
    if (!ltlib::makeSingletonProcess("lanthing_app")) {
        printf("Another instance is running.\n");
        return 0;
    }
    auto now = time(nullptr);
    ::srand(now);
    initLoggingAndDumps();
    std::unique_ptr<lt::App> app = lt::App::create();
    if (app == nullptr) {
        return -1;
    }
    LOG(INFO) << "app run.";
    return app->exec(argc, argv);
}
