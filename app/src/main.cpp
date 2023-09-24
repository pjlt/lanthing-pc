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

// 太丑了，应该在LogSink里实现
void logfileHelper(const std::filesystem::path& directory, const std::string& logger_id) {
    time_t s = ::time(nullptr);
    struct tm last_tm = *localtime(&s);
    last_tm.tm_mday += 1;
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds{1});
        time_t totalseconds = ::time(nullptr);
        struct tm curr_tm = *localtime(&totalseconds);
        curr_tm.tm_year += 1900;
        curr_tm.tm_mday += 1;
        if (curr_tm.tm_mday != last_tm.tm_mday) {
            // 1. 滚动
            if (g_logsSink) {
                auto sink = g_logsSink->sink().lock();
                sink->_real_sink->changeLogFile(directory.string(), logger_id);
            }
            last_tm = curr_tm;
            // 2. 删除
            auto expire_tp = std::chrono::system_clock::now() - std::chrono::hours{24 * 7};
            auto expire_time_t = std::chrono::system_clock::to_time_t(expire_tp);
            struct tm expire_tm = *localtime(&expire_time_t);
            expire_tm.tm_year += 1900;
            expire_tm.tm_mday += 1;
            int expire_date =
                expire_tm.tm_year * 10000 + (expire_tm.tm_mon + 1) * 100 + expire_tm.tm_mday;
            std::regex pattern{".+?([0-9]+?)-.+?"};
            for (const auto& file : std::filesystem::directory_iterator{directory}) {

                std::smatch sm;
                std::string filename = file.path().string();
                if (!std::regex_match(filename, sm, pattern)) {
                    continue;
                }
                int file_date = atoi(sm[1].str().c_str());
                if (file_date < expire_date) {
                    remove(filename.c_str());
                }
            }
        }
    }
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
    g_logsSink = g_logWorker->addDefaultLogger(kPrefix, log_dir.string(), "app");
    g3::initializeLogging(g_logWorker.get());
    std::thread([log_dir]() { logfileHelper(log_dir, "app"); }).detach();
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
