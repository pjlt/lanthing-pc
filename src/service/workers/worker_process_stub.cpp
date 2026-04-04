#include "worker_process.h"

#include <ltlib/logging.h>

namespace lt {

namespace svc {

std::unique_ptr<WorkerProcess> WorkerProcess::create(const Params&) {
    LOG(ERR) << "WorkerProcess is only available on Windows";
    return nullptr;
}

WorkerProcess::WorkerProcess(const Params& params)
    : path_{params.path}
    , pipe_name_{params.pipe_name}
    , client_width_{params.client_width}
    , client_height_{params.client_height}
    , client_refresh_rate_{params.client_refresh_rate}
    , color_matrix_{params.color_matrix}
    , full_range_{params.full_range}
    , client_video_codecs_{params.client_video_codecs}
    , audio_codec_{params.audio_codec}
    , on_failed_{params.on_failed}
    , run_as_win_service_{false} {}

WorkerProcess::~WorkerProcess() = default;

void WorkerProcess::stop() {
    stoped_ = true;
}

void WorkerProcess::changeResolution(uint32_t width, uint32_t height, uint32_t monitor_index) {
    client_width_ = width;
    client_height_ = height;
    monitor_index_ = monitor_index;
}

void WorkerProcess::start() {}

void WorkerProcess::mainLoop(const std::function<void()>&) {}

bool WorkerProcess::launchWorkerProcess() {
    return false;
}

bool WorkerProcess::waitForWorkerProcess(const std::function<void()>&) {
    return false;
}

} // namespace svc

} // namespace lt
