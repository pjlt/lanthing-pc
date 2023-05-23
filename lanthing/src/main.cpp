#include <map>
#include <string>
#include <iostream>
#include <vector>
#include <filesystem>
#include <g3log/g3log.hpp>
#include <g3log/logworker.hpp>
#include <ltlib/system.h>
#include <ltlib/event.h>
#include <client/client.h>
#include <worker/worker.h>
#if defined(RUN_AS_SERVICE)
#include <service/daemon/daemon.h>
#else
#include <service/service.h>
#endif

namespace
{

enum class Role
{
    Service,
    Client,
    Worker,
};

auto init_log(Role role) -> std::tuple<std::unique_ptr<g3::LogWorker>, std::unique_ptr<g3::FileSinkHandle>>
{
    std::string prefix;
    std::string rtc_prefix;
    std::filesystem::path log_dir;
    uint32_t reverse_days = 7;
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
        return {};
    }

    std::string bin_path = ltlib::get_program_fullpath<char>();
    std::string bin_dir = ltlib::get_program_path<char>();
    std::string appdata_dir = ltlib::get_appdata_path(ltlib::is_run_as_service());
    if (!appdata_dir.empty()) {
        log_dir = appdata_dir;
        log_dir /= "lanthing";
        log_dir /= prefix;
    } else {
        log_dir = bin_dir;
        log_dir /= "log";
    }
    if (!std::filesystem::exists(log_dir)) {
        if (!std::filesystem::create_directories(log_dir)) {
            ::printf("Create log directory '%s' failed\n", log_dir.string().c_str());
        }
    }
    auto g3worker = g3::LogWorker::createLogWorker();
    auto g3sink = g3worker->addDefaultLogger(prefix, log_dir.string());
    g3::initializeLogging(g3worker.get());
    LOG(INFO) << "Log system initialized";

    //if ((role == Role::Service || role == Role::Client) && !rtc_prefix.empty()) {
    //    ltrtc::init_logging(log_dir.string(), rtc_prefix);
    //}

    return std::make_tuple(std::move(g3worker), std::move(g3sink));
}

std::map<std::string, std::string> parse_options(int argc, char* argv[])
{
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
            options.insert({ args[i], args[i + 1] });
            ++i;
        }
    }
    return options;
}

bool is_another_instance_running()
{
    namespace fs = std::filesystem;
    fs::path program_full_path { ltlib::get_program_fullpath<char>() };
#ifdef LT_WINDOWS
    std::string event_name = std::string { "Global\\" } + program_full_path.filename().string() + ".tsa";
#else
    std::string event_name = "xxxx";
#endif
    ltlib::Event singleton { event_name };
    if (!singleton.is_owner()) {
        LOG(WARNING) << "Another instance is running";
        return true;
    } else {
        return false;
    }
}

int run_as_client(std::map<std::string, std::string> options)
{
    auto [g3worker, g3sink] = init_log(Role::Client);
    auto client = lt::cli::Client::create(options);
    if (client) {
        client->wait();
        return 0;
    } else {
        return -1;
    }
}

int run_as_service(std::map<std::string, std::string> options)
{
    auto [g3worker, g3sink] = init_log(Role::Service);
    if (is_another_instance_running()) {
        return -1;
    }
#if defined(LT_WINDOWS) && defined(RUN_AS_SERVICE)
    lt::svc::LanthingWinService svc;
    ltlib::ServiceApp app { &svc };
    app.run();
#else
    lt::svc::Service svc;
    if (!svc.init()) {
        return -1;
    }
    svc.uninit();
#endif

    LOG(INFO) << "Normal exit";
    return 0;
}

int run_as_worker(std::map<std::string, std::string> options)
{
    auto [g3worker, g3sink] = init_log(Role::Worker);
    auto worker = lt::worker::Worker::create(options);
    if (worker) {
        worker->wait();
        return 0;
    } else {
        return -1;
    }
}

} // 匿名空间


int main(int argc, char* argv[])
{
    auto options = parse_options(argc, argv);
    auto iter = options.find("-type");
    if (iter == options.end() || iter->second == "service") {
        return run_as_service(options);
    } else if (iter->second == "client") {
        // 方便调试attach
        // std::this_thread::sleep_for(std::chrono::seconds { 15 });
        return run_as_client(options);
    } else if (iter->second == "worker") {
        return run_as_worker(options);
    } else {
        std::cerr << "Unknown type '" << iter->second << "'" << std::endl;
        return -1;
    }
}