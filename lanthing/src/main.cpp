#include <filesystem>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include <g3log/g3log.hpp>
#include <g3log/logworker.hpp>

#include <lt_minidump_generator.h>
#include <ltlib/event.h>
#include <ltlib/system.h>
#include <ltlib/threads.h>
#include <rtc/rtc.h>

#include <client/client.h>
#include <worker/worker.h>
#if LT_RUN_AS_SERVICE
#include <service/daemon/daemon.h>
#else
#include <service/service.h>
#endif

#include "firewall.h"

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

void init_log_and_minidump(Role role) {
    std::string prefix;
    std::string rtc_prefix;
    std::filesystem::path log_dir;
    // uint32_t reverse_days = 7; // TODO: 删除旧日志
    switch (role) {
    case Role::Client:
        prefix = "client";
        rtc_prefix = "ltcli_";
        break;
    case Role::Service:
        prefix = "service";
        rtc_prefix = "ltsvr_";
        break;
    case Role::Worker:
        prefix = "worker";
        break;
    default:
        std::cout << "Unknown process role " << static_cast<int>(role) << std::endl;
        return;
    }

    std::string bin_path = ltlib::get_program_fullpath<char>();
    std::string bin_dir = ltlib::get_program_path<char>();
    std::string appdata_dir = ltlib::get_appdata_path(ltlib::is_run_as_service());
    if (!appdata_dir.empty()) {
        log_dir = appdata_dir;
        log_dir /= "lanthing";
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
    g_log_sink = g_log_worker->addDefaultLogger(prefix, log_dir.string(), "lanthing");
    g3::initializeLogging(g_log_worker.get());
    ltlib::ThreadWatcher::instance()->register_terminate_callback(
        [](const std::string& last_word) { LOG(INFO) << "Last words: " << last_word; });
    if ((role == Role::Service || role == Role::Client) && !rtc_prefix.empty()) {
        rtc::initLogging(log_dir.string().c_str(), rtc_prefix.c_str());
    }
    LOG(INFO) << "Log system initialized";

    // g3log必须再minidump前初始化
    g_minidump_genertator = std::make_unique<LTMinidumpGenerator>(log_dir.string());
    signal(SIGINT, sigint_handler);
    // ltlib::ThreadWatcher::instance()->disable_crash_on_timeout();
}

std::map<std::string, std::string> parse_options(int argc, char* argv[]) {
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

bool is_another_instance_running() {
    namespace fs = std::filesystem;
    fs::path program_full_path{ltlib::get_program_fullpath<char>()};
#ifdef LT_WINDOWS
    std::string event_name =
        std::string{"Global\\"} + program_full_path.filename().string() + ".tsa";
#else
    std::string event_name = "xxxx";
#endif
    ltlib::Event singleton{event_name};
    if (!singleton.is_owner()) {
        LOG(WARNING) << "Another instance is running";
        return true;
    }
    else {
        return false;
    }
}

int run_as_client(std::map<std::string, std::string> options) {
    init_log_and_minidump(Role::Client);
    lt::create_inbound_firewall_rule("Lanthing", ltlib::get_program_fullpath<char>());
    auto client = lt::cli::Client::create(options);
    if (client) {
        client->wait();
        return 0;
    }
    else {
        return -1;
    }
}

int run_as_service(std::map<std::string, std::string> options) {
    init_log_and_minidump(Role::Service);
    lt::create_inbound_firewall_rule("Lanthing", ltlib::get_program_fullpath<char>());
    if (is_another_instance_running()) {
        return -1;
    }
#if defined(LT_WINDOWS) && LT_RUN_AS_SERVICE
    lt::svc::LanthingWinService svc;
    ltlib::ServiceApp app{&svc};
    app.run();
#else
    lt::svc::Service svc;
    if (!svc.init()) {
        return -1;
    }
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds{10000});
    }
    svc.uninit();
#endif

    LOG(INFO) << "Normal exit";
    return 0;
}

int run_as_worker(std::map<std::string, std::string> options) {
    init_log_and_minidump(Role::Worker);
    auto worker = lt::worker::Worker::create(options);
    if (worker) {
        worker->wait();
        LOG(INFO) << "Normal exit";
        return 0;
    }
    else {
        return -1;
    }
}

} // namespace

int main(int argc, char* argv[]) {
    auto options = parse_options(argc, argv);
    auto iter = options.find("-type");
    if (iter == options.end() || iter->second == "service") {
        return run_as_service(options);
    }
    else if (iter->second == "client") {
        // 方便调试attach
        // std::this_thread::sleep_for(std::chrono::seconds { 15 });
        return run_as_client(options);
    }
    else if (iter->second == "worker") {
        // std::this_thread::sleep_for(std::chrono::seconds { 15 });
        return run_as_worker(options);
    }
    else {
        std::cerr << "Unknown type '" << iter->second << "'" << std::endl;
        return -1;
    }
}