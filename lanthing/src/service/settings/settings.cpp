#include <sstream>
#include <filesystem>
#include "settings.h"
#include <ltlib/system.h>
#include <toml++/toml.h>

namespace lt
{

namespace svc
{

class SettingsToml : public Settings
{
public:
    SettingsToml() = default;
    ~SettingsToml() override = default;
    bool init() override;
    void set_boolean(const std::string& key, bool value) override;
    auto get_boolean(const std::string& key) -> std::optional<bool> override;
    void set_integer(const std::string& key, int64_t value) override;
    auto get_integer(const std::string& key) -> std::optional<int64_t> override;
    void set_string(const std::string& key, const std::string& value) override;
    auto get_string(const std::string& key) -> std::optional<std::string> override;

private:
    std::mutex mutex_;
    std::fstream file_;
    std::string text_;
};

bool SettingsToml::init()
{
    std::string appdatapath = ltlib::get_appdata_path(ltlib::is_run_as_service());
    if (appdatapath.empty()) {
        return false;
    }
    std::filesystem::path filepath = appdatapath;
    filepath = filepath / "lanthing" / "settings.toml";
    file_ = std::fstream { filepath.string(), std::ios::in | std::ios::out | std::ios::app };
    if (file_.fail()) {
        return false;
    }
    std::stringstream ss;
    ss << file_.rdbuf();
    text_ = ss.str();
    file_.close();
    file_ = std::fstream { filepath.string(), std::ios::in | std::ios::out };
    return true;
}

void SettingsToml::set_boolean(const std::string& key, bool value)
{
    std::lock_guard<std::mutex> lk { mutex_ };
    auto settings = toml::parse(text_);
    settings.insert_or_assign(key, value);
    std::stringstream ss;
    ss << toml::toml_formatter { settings };
    text_ = ss.str();
    file_.seekp(std::ios::beg);
    file_.write(text_.c_str(), text_.size());
}

auto SettingsToml::get_boolean(const std::string& key) -> std::optional<bool>
{
    std::lock_guard<std::mutex> lk { mutex_ };
    auto settings = toml::parse(text_);
    auto value = settings[key].value<bool>();
    return value;
}

void SettingsToml::set_integer(const std::string& key, int64_t value)
{
    std::lock_guard<std::mutex> lk { mutex_ };
    auto settings = toml::parse(text_);
    settings.insert_or_assign(key, value);
    std::stringstream ss;
    ss << toml::toml_formatter { settings };
    text_ = ss.str();
    file_.seekp(std::ios::beg);
    file_.write(text_.c_str(), text_.size());
    file_.flush();
}

auto SettingsToml::get_integer(const std::string& key) -> std::optional<int64_t>
{
    std::lock_guard<std::mutex> lk { mutex_ };
    auto settings = toml::parse(text_);
    auto value = settings[key].value<int64_t>();
    return value;
}

void SettingsToml::set_string(const std::string& key, const std::string& value)
{
    std::lock_guard<std::mutex> lk { mutex_ };
    auto settings = toml::parse(text_);
    settings.insert_or_assign(key, value);
    std::stringstream ss;
    ss << toml::toml_formatter { settings };
    text_ = ss.str();
    file_.seekp(std::ios::beg);
    file_.write(text_.c_str(), text_.size());
}

auto SettingsToml::get_string(const std::string& key) -> std::optional<std::string>
{
    std::lock_guard<std::mutex> lk { mutex_ };
    auto settings = toml::parse(text_);
    auto value = settings[key].value<std::string>();
    return value;
}

std::unique_ptr<Settings> Settings::create(Storage type)
{
    std::unique_ptr<Settings> settings;
    switch (type) {
    case Settings::Storage::Toml:
        settings = std::make_unique<SettingsToml>();
        break;
    case Settings::Storage::Sqlite:
        break;
    default:
        break;
    }
    if (settings == nullptr) {
        return nullptr;
    }
    if (!settings->init()) {
        return nullptr;
    }
    return settings;
}

} // namespace svc

} // namespace lt