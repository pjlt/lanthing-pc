#pragma once
#include <ltlib/ltlib.h>
#include <cstdint>
#include <fstream>
#include <memory>
#include <mutex>
#include <optional>
#include <string>

namespace ltlib
{

class LT_API Settings
{
public:
    enum class Storage
    {
        Toml,
        Sqlite,
    };

public:
    virtual ~Settings() = default;
    static std::unique_ptr<Settings> create(Storage type);
    virtual void set_boolean(const std::string& key, bool value) = 0;
    virtual auto get_boolean(const std::string& key) -> std::optional<bool> = 0;
    virtual void set_integer(const std::string& key, int64_t value) = 0;
    virtual auto get_integer(const std::string& key) -> std::optional<int64_t> = 0;
    virtual void set_string(const std::string& key, const std::string& value) = 0;
    virtual auto get_string(const std::string& key) -> std::optional<std::string> = 0;

protected:
    virtual bool init() = 0;
};

} // namespace ltlib