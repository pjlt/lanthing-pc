#include "client.h"
#include <g3log/g3log.hpp>

namespace lt
{

std::unique_ptr<Client> Client::create(std::map<std::string, std::string> options)
{
    if (options.find("-cid") == options.end()
        || options.find("-rid") == options.end()
        || options.find("-token") == options.end()
        || options.find("-user") == options.end()
        || options.find("-pwd") == options.end()
        || options.find("-addr") == options.end()
        || options.find("-port") == options.end()
        || options.find("-codec") == options.end()
        || options.find("-width") == options.end()
        || options.find("-height") == options.end()
        || options.find("-freq") == options.end()
        || options.find("-dinput") == options.end()
        || options.find("-gamepad") == options.end()) {
        LOG(WARNING) << "Parameter invalid";
        return nullptr;
    }
    Params params {};
    params.client_id = options["-cid"];
    params.room_id = options["-rid"];
    params.auth_token = options["-token"];
    params.signaling_addr = options["-addr"];
    params.user = options["-user"];
    params.pwd = options["-pwd"];
    int32_t signaling_port = std::atoi(options["-port"].c_str());
    params.codec = options["-codec"];
    int32_t width = std::atoi(options["-width"].c_str());
    int32_t height = std::atoi(options["-height"].c_str());
    int32_t freq = std::atoi(options["-freq"].c_str());
    params.enable_driver_input = std::atoi(options["-dinput"].c_str()) != 0;
    params.enable_gamepad = std::atoi(options["-gamepad"].c_str()) != 0;
    if (signaling_port <= 0 || signaling_port > 65535) {
        LOG(WARNING) << "Invalid parameter: port";
        return nullptr;
    }
    params.signaling_port = static_cast<uint16_t>(signaling_port);

    if (width <= 0) {
        LOG(WARNING) << "Invalid parameter: width";
        return nullptr;
    }
    params.width = static_cast<uint32_t>(width);

    if (height <= 0) {
        LOG(WARNING) << "Invalid parameter: height";
        return nullptr;
    }
    params.height = static_cast<uint32_t>(height);

    if (freq <= 0) {
        LOG(WARNING) << "Invalid parameter: freq";
        return nullptr;
    }
    params.screen_refresh_rate = static_cast<uint32_t>(freq);

    std::unique_ptr<Client> client { new Client {params} };
    if (!client->init()) {
        return false;
    }
    return client;
}

Client::Client(const Params& params)
{
}

bool Client::init()
{
    return false;
}

} // namespace lt