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
    virtual void setBoolean(const std::string& key, bool value) = 0;
    virtual auto getBoolean(const std::string& key) -> std::optional<bool> = 0;
    virtual void setInteger(const std::string& key, int64_t value) = 0;
    virtual auto getInteger(const std::string& key) -> std::optional<int64_t> = 0;
    virtual void setString(const std::string& key, const std::string& value) = 0;
    virtual auto getString(const std::string& key) -> std::optional<std::string> = 0;

protected:
    virtual bool init() = 0;
};

} // namespace ltlib