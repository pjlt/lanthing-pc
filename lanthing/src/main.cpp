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

#include <filesystem>
#include <iostream>
#include <map>
#include <regex>
#include <string>
#include <vector>

#include <g3log/logworker.hpp>
#include <lt_minidump_generator.h>

#include <ltlib/event.h>
#include <ltlib/logging.h>
#include <ltlib/singleton_process.h>
#include <ltlib/strings.h>
#include <ltlib/system.h>
#include <ltlib/threads.h>

#include <client/client.h>
#include <worker/worker.h>
#if defined(LT_WINDOWS) && LT_RUN_AS_SERVICE
#include <service/daemon/daemon.h>
#else
#include <service/service.h>
#endif

#include <rtc/rtc.h>

#include "firewall.h"
#include "lt_constants.h"

#if defined(LT_WINDOWS) && LT_RUN_AS_SERVICE
// 不显示命令行窗口.
#pragma comment(linker, "/subsystem:\"windows\" /entry:\"mainCRTStartup\"")
#endif

namespace {

enum class Role {
    Service,
    Client,
    Worker,
};

std::unique_ptr<g3::LogWorker> g_log_worker;
std::unique_ptr<g3::FileSinkHandle> g_log_sink;
std::unique_ptr<LTMinidumpGenerator> g_minidump_genertator;

void sigint_handler(int) {
    LOG(INFO) << "SIGINT Received";
    g_log_worker.reset();
    g_log_sink.reset();
    g_minidump_genertator.reset();
    // std::terminate();
    ::exit(0);
}

void terminateCallback(const std::string& last_word) {
    LOG(INFO) << "Last words: " << last_word;
}

void cleanupDumps(const std::filesystem::path& path) {
    return;
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

void initLogAndMinidump(Role role) {
    std::string prefix;
    std::string rtc_prefix;
    std::filesystem::path log_dir;
    switch (role) {
    case Role::Client:
        prefix = "client";
        rtc_prefix = "rtccli.";
        break;
    case Role::Service:
        prefix = "service";
        rtc_prefix = "rtcsvr.";
        break;
    case Role::Worker:
        prefix = "worker";
        break;
    default:
        std::cout << "Unknown process role " << static_cast<int>(role) << std::endl;
        return;
    }

    std::string bin_path = ltlib::getProgramFullpath();
    std::string bin_dir = ltlib::getProgramPath();
    std::string appdata_dir = ltlib::getConfigPath(true);
    std::wstring w_appdata_dir = ltlib::utf8To16(appdata_dir);
    if (!w_appdata_dir.empty()) {
        log_dir = w_appdata_dir;
        log_dir /= "log";
        log_dir /= prefix;
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
    g_log_worker = g3::LogWorker::createLogWorker();
    g_log_worker->addSink(
        std::make_unique<ltlib::LogSink>(prefix, log_dir.string(), 30 /*flush_every_30_logs*/),
        &ltlib::LogSink::fileWrite);
    g3::log_levels::disable(DEBUG);
    g3::only_change_at_initialization::addLogLevel(ERR);
    g3::initializeLogging(g_log_worker.get());
    if ((role == Role::Service || role == Role::Client) && !rtc_prefix.empty()) {
        rtc::initLogging(log_dir.string().c_str(), rtc_prefix.c_str());
    }
    LOGF(INFO, "Lanthing Version: v%d.%d.%d, Build time: %s %s", LT_VERSION_MAJOR, LT_VERSION_MINOR,
         LT_VERSION_PATCH, __DATE__, __TIME__);

    std::thread cleanup_dumps([log_dir]() { cleanupDumps(log_dir); });
    cleanup_dumps.detach();

    // g3log必须再minidump前初始化
#if LT_WINDOWS
    g_minidump_genertator = std::make_unique<LTMinidumpGenerator>(
        ltlib::utf8To16(log_dir.string()), ltlib::utf8To16(ltlib::getProgramName()));
#else
    g_minidump_genertator =
        std::make_unique<LTMinidumpGenerator>(log_dir.string(), ltlib::getProgramName());
#endif
    g_minidump_genertator->addCallback([]() { rtc::flushLogs(); });
    signal(SIGINT, sigint_handler);
    if (LT_CRASH_ON_THREAD_HANGS) {
        ltlib::ThreadWatcher::instance()->enableCrashOnTimeout();
        ltlib::ThreadWatcher::instance()->registerTerminateCallback(terminateCallback);
    }
    else {
        ltlib::ThreadWatcher::instance()->disableCrashOnTimeout();
    }
}

std::map<std::string, std::string> parseOptions(int argc, char* argv[]) {
    std::vector<std::string> args;
    std::map<std::string, std::string> options;
    for (int i = 0; i < argc; i++) {
        args.push_back(argv[i]);
    }
    for (size_t i = 0; i < args.size(); ++i) {
        if ('-' != args[i][0]) {
            continue;
        }
        if (i >= args.size() - 1) {
            break;
        }
        if ('-' != args[i + 1][0]) {
            options.insert({args[i], args[i + 1]});
            ++i;
        }
    }
    return options;
}

int runAsClient(std::map<std::string, std::string> options) {
    initLogAndMinidump(Role::Client);
    lt::createInboundFirewallRule("Lanthing", ltlib::getProgramFullpath());
    auto client = lt::cli::Client::create(options);
    if (client) {
        client->wait();
        return 0;
    }
    else {
        return 1;
    }
}

int runAsService(std::map<std::string, std::string> options) {
    (void)options;
#if defined(LT_WINDOWS)
    if (!ltlib::makeSingletonProcess("lanthing")) {
        printf("Another instance is running.\n");
        return -1;
    }
    initLogAndMinidump(Role::Service);
    lt::createInboundFirewallRule("Lanthing", ltlib::getProgramFullpath());
#if LT_RUN_AS_SERVICE
    lt::svc::LanthingWinService svc;
    ltlib::ServiceApp app{&svc};
    app.run();
#else  // LT_RUN_AS_SERVICE
    lt::svc::Service svc;
    if (!svc.init()) {
        return 1;
    }
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds{10000});
    }
    svc.uninit();
#endif // LT_RUN_AS_SERVICE
    LOG(INFO) << "Normal exit";
    return 0;
#else // LT_WINDOWS
    printf("Unavailable 'runAsService' for current platform\n");
    return 1;
#endif
}

int runAsWorker(std::map<std::string, std::string> options) {
    (void)options;
#if defined(LT_WINDOWS) || defined(LT_LINUX)
    initLogAndMinidump(Role::Worker);
    auto [worker, exit_code] = lt::worker::Worker::create(options);
    if (worker) {
        int ret = worker->wait();
        LOG(INFO) << "Normal exit " << ret;
        return ret;
    }
    else {
        LOG(INFO) << "Exit with failure";
        // worker初始化失败的错误码
        return exit_code;
    }
#else  // LT_WINDOWS
    printf("Unavailable 'runAsWorker' for current platform\n");
    return -1;
#endif // LT_WINDOWS
}

} // namespace

int main(int argc, char* argv[]) {
    ::srand(static_cast<unsigned int>(::time(nullptr)));
    auto options = parseOptions(argc, argv);
    auto iter = options.find("-type");
    if (iter == options.end() || iter->second == "service") {
        return runAsService(options);
    }
    else if (iter->second == "client") {
        // 方便调试attach
        // std::this_thread::sleep_for(std::chrono::seconds{15});
        return runAsClient(options);
    }
    else if (iter->second == "worker") {
        // std::this_thread::sleep_for(std::chrono::seconds { 15 });
        return runAsWorker(options);
    }
    else {
        std::cerr << "Unknown type '" << iter->second << "'" << std::endl;
        return -1;
    }
}