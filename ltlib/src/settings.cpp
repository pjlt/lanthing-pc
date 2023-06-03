#include <ltlib/settings.h>

#include <filesystem>
#include <sstream>

#include <toml++/toml.h>

#include <ltlib/system.h>
#include <ltlib/times.h>

namespace
{

constexpr uint32_t k5ms = 5;

class UniqueFile
{
public:
    static std::unique_ptr<UniqueFile> open(const std::string& path);
    std::string read_str();
    void write_str(const std::string& str);

private:
    std::fstream file_;
};

std::unique_ptr<UniqueFile> UniqueFile::open(const std::string& path)
{
    // linux用flock()
    std::fstream file;
    file.rdbuf()->open(path, std::ios::in | std::ios::out, _SH_DENYRW);
    if (file.fail()) {
        return nullptr;
    }
    auto uf = std::make_unique<UniqueFile>();
    uf->file_ = std::move(file);
    return uf;
}

std::string UniqueFile::read_str()
{
    std::stringstream ss;
    ss << file_.rdbuf();
    return ss.str();
}

void UniqueFile::write_str(const std::string& str)
{
    file_.seekp(std::ios::beg);
    file_.write(str.c_str(), str.size());
    file_.flush();
}

} // namespace

namespace ltlib
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
    template <typename VType>
    void set_value(const std::string& key, VType value)
    {
        std::lock_guard<std::mutex> lk { mutex_ };
        std::unique_ptr<UniqueFile> file;
        if (ltlib::steady_now_ms() - last_read_time_ms_ >= k5ms) {
            file = UniqueFile::open(filepath_);
            if (file == nullptr) {
                return;
            }
            text_ = file->read_str();
            last_read_time_ms_ = ltlib::steady_now_ms();
        }
        auto settings = toml::parse(text_);
        settings.insert_or_assign(key, value);
        std::stringstream ss;
        ss << toml::toml_formatter { settings };
        text_ = ss.str();
        file->write_str(text_);
    }

    template <typename VType>
    std::optional<VType> get_value(const std::string& key)
    {
        std::lock_guard<std::mutex> lk { mutex_ };
        std::unique_ptr<UniqueFile> file;
        if (ltlib::steady_now_ms() - last_read_time_ms_ >= k5ms) {
            file = UniqueFile::open(filepath_);
            if (file == nullptr) {
                return {};
            }
            text_ = file->read_str();
            last_read_time_ms_ = ltlib::steady_now_ms();
        }
        auto settings = toml::parse(text_);
        auto value = settings[key].value<VType>();
        return value;
    }

private:
    std::mutex mutex_;
    std::string text_;
    int64_t last_read_time_ms_ = 0;
    std::string filepath_;
};

bool SettingsToml::init()
{
    std::string appdatapath = ltlib::get_appdata_path(ltlib::is_run_as_service());
    if (appdatapath.empty()) {
        return false;
    }
    std::filesystem::path filepath = appdatapath;
    filepath = filepath / "lanthing" / "settings.toml";
    filepath_ = filepath.string();
    auto file = std::fstream { filepath_, std::ios::in | std::ios::out | std::ios::app };
    if (file.fail()) {
        return false;
    }
    std::stringstream ss;
    ss << file.rdbuf();
    text_ = ss.str();
    last_read_time_ms_ = ltlib::steady_now_ms();
    return true;
}

void SettingsToml::set_boolean(const std::string& key, bool value)
{
    set_value(key, value);
}

auto SettingsToml::get_boolean(const std::string& key) -> std::optional<bool>
{
    auto value = get_value<bool>(key);
    return value;
}

void SettingsToml::set_integer(const std::string& key, int64_t value)
{
    set_value(key, value);
}

auto SettingsToml::get_integer(const std::string& key) -> std::optional<int64_t>
{
    auto value = get_value<int64_t>(key);
    return value;
}

void SettingsToml::set_string(const std::string& key, const std::string& value)
{
    set_value(key, value);
}

auto SettingsToml::get_string(const std::string& key) -> std::optional<std::string>
{
    auto value = get_value<std::string>(key);
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

} // namespace ltlib