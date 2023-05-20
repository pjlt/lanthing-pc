#include "worker.h"
#include <g3log/g3log.hpp>

namespace lt
{

std::unique_ptr<Worker> Worker::create(std::map<std::string, std::string> options)
{
    if (options.find("-width") == options.end()
        || options.find("-height") == options.end()
        || options.find("-freq") == options.end()
        || options.find("-codecs") == options.end()) {
        LOG(WARNING) << "Parameter invalid";
        return nullptr;
    }
    Params params {};
    int32_t width = std::atoi(options["-width"].c_str());
    int32_t height = std::atoi(options["-height"].c_str());
    int32_t freq = std::atoi(options["-freq"].c_str());
    params.codecs = options["-codecs"];
    if (width <= 0) {
        LOG(WARNING) << "Parameter invalid: width";
        return nullptr;
    }
    params.width = static_cast<uint32_t>(width);
    if (height <= 0) {
        LOG(WARNING) << "Parameter invalid: height";
        return nullptr;
    }
    params.height = static_cast<uint32_t>(height);
    if (freq <= 0) {
        LOG(WARNING) << "Parameter invalid: freq";
        return nullptr;
    }
    params.refresh_rate = static_cast<uint32_t>(freq);


    std::unique_ptr<Worker> worker { new Worker {params} };
    if (!worker->init()) {
        return nullptr;
    }
    return worker;
}

Worker::Worker(const Params& params)
{
    //
}

bool Worker::init()
{
    return false;
}

} // namespace lt