#pragma once
#include <string>
#include <map>
#include <memory>

namespace lt
{

class Worker
{
public:
    struct Params
    {
        std::string rname;
        uint32_t rsize;
        std::string wname;
        uint32_t wsize;
        uint32_t width;
        uint32_t height;
        uint32_t refresh_rate;
        std::string codecs;
    };

public:
    static std::unique_ptr<Worker> create(std::map<std::string, std::string> options);
    ~Worker();
    void wait();

private:
    Worker(const Params& params);
    bool init();
    void uninit();
};

} // namespace lt