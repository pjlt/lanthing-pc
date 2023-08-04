#include "app.h"

#include <cstdarg>
#include <filesystem>
#include <memory>

#include <g3log/g3log.hpp>
#include <g3log/logworker.hpp>
#include <lt_minidump_generator.h>

#include <ltlib/system.h>
#include <ltlib/threads.h>

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
    std::string bin_path = ltlib::get_program_fullpath<char>();
    std::string bin_dir = ltlib::get_program_path<char>();
    std::string appdata_dir = ltlib::get_appdata_path(/*is_win_service=*/false);
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
    ltlib::ThreadWatcher::instance()->register_terminate_callback(
        [](const std::string& last_word) { LOG(INFO) << "Last words: " << last_word; });

    LOG(INFO) << "Log system initialized";

    g_minidumpGenertator = std::make_unique<LTMinidumpGenerator>(log_dir.string());
    signal(SIGINT, sigint_handler);
}

} // namespace

int main(int argc, char** argv) {
    initLogging();
    std::unique_ptr<lt::App> app = lt::App::create();
    if (app == nullptr) {
        return -1;
    }
    LOG(INFO) << "app run.";
    return app->exec(argc, argv);
}
