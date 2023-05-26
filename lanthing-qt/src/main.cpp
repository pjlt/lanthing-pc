#include <cstdint>
#include <string>
#include <iostream>
#include <filesystem>

#include <g3log/g3log.hpp>
#include <g3log/logworker.hpp>

#include <ltlib/system.h>
#include <ltlib/threads.h>

std::unique_ptr<g3::LogWorker> g_log_worker;
std::unique_ptr<g3::FileSinkHandle> g_log_sink;

void sigint_handler(int)
{
    LOG(INFO) << "SIGINT Received";
    g_log_worker.reset();
    g_log_sink.reset();
    std::terminate();
}

void init_log()
{
    const std::string kPreifix = "gui";
    std::filesystem::path log_dir;
    std::string bin_path = ltlib::get_program_fullpath<char>();
    std::string bin_dir = ltlib::get_program_path<char>();
    std::string appdata_dir = ltlib::get_appdata_path(ltlib::is_run_as_service());
    if (!appdata_dir.empty()) {
        log_dir = appdata_dir;
        log_dir /= "lanthing";
        log_dir /= kPreifix;
    } else {
        log_dir = bin_dir;
        log_dir /= "log";
    }
    if (!std::filesystem::exists(log_dir)) {
        if (!std::filesystem::create_directories(log_dir)) {
            ::printf("Create log directory '%s' failed\n", log_dir.string().c_str());
        }
    }
    g_log_worker = g3::LogWorker::createLogWorker();
    g_log_sink = g_log_worker->addDefaultLogger(kPreifix, log_dir.string());
    g3::initializeLogging(g_log_worker.get());
    LOG(INFO) << "Log system initialized";

    signal(SIGINT, sigint_handler);
    ltlib::ThreadWatcher::instance()->disable_crash_on_timeout();
}

#include "client_ui.h"

int main()
{
    init_log();
    int64_t kMyDeviceID = 1234567;
    int64_t kPeerDeviceID = 1234567;
    lt::ui::ClientUI client_ui;
    if (!client_ui.start(kMyDeviceID, kPeerDeviceID)) {
        return -1;
    }
    client_ui.wait();
    return 0;
}