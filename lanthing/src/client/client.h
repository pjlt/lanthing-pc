#pragma once
#include <cstdint>
#include <memory>
#include <map>
#include <string>

namespace lt
{

class Client
{
public:
    struct Params
    {
        std::string client_id;
        std::string room_id;
        std::string auth_token;
        std::string user;
        std::string pwd;
        std::string signaling_addr;
        uint16_t signaling_port;
        std::string codec;
        uint32_t width;
        uint32_t height;
        uint32_t screen_refresh_rate;
        bool enable_driver_input;
        bool enable_gamepad;
    };
public:
    static std::unique_ptr<Client> create(std::map<std::string, std::string> options);
    ~Client();
    void wait();

private:
    Client(const Params& params);
    bool init();

private:
};

} // namespace lt