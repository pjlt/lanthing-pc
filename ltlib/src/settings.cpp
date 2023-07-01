#if defined(LT_WINDOWS)
#include <Windows.h>
#elif defined(LT_LINUX)
#include <fcntl.h>
#else
#error unsupported platform
#endif
#include <ltlib/settings.h>

#include <filesystem>
#include <sstream>

#include <toml++/toml.h>

#include <ltlib/system.h>
#include <ltlib/times.h>
#include <ltlib/strings.h>

namespace
{

constexpr uint32_t k5ms = 5;

#if defined(LT_WINDOWS)
// Windows implementation
class FileMutex
{
public:
    FileMutex(HANDLE handle)
        : handle_ { handle }
    {
    }
    void lock()
    {
        BOOL ret;
        DWORD size_low, size_high;
        OVERLAPPED ol;
        int flags = 0;
        size_low = GetFileSize(handle_, &size_high);
        ::memset(&ol, 0, sizeof(OVERLAPPED));
        ret = LockFileEx(handle_, LOCKFILE_EXCLUSIVE_LOCK, 0, size_low, size_high, &ol);
    }
    void unlock()
    {
        BOOL ret;
        DWORD size_low, size_high;
        size_low = GetFileSize(handle_, &size_high);
        ret = UnlockFile(handle_, 0, 0, size_low, size_high);
    }

private:
    HANDLE handle_;
};
#elif defined(LT_LINUX)
// Linux implementation
class FileMutex
{
public:
    FileMutex(int fd)
        : fd_ { fd }
    {
    }
    void lock()
    {
    }
    void unlock()
    {
    }

private:
    int fd_;
};
#else
#error unsupported platform
#endif

class LockedFile
{
public:
    ~LockedFile();
    static std::unique_ptr<LockedFile> open(const std::string& path);
    std::string read_str();
    void write_str(const std::string& str);

private:
#if defined(LT_WINDOWS)
    // Windows implementation
    HANDLE handle_ = nullptr;
#elif defined(LT_LINUX)
    // Linux implementation
    int fd_ = -1;
#else
#error unsupported platform
#endif
    std::unique_ptr<FileMutex> mutex_;
};

std::unique_ptr<LockedFile> LockedFile::open(const std::string& path)
{
#if defined(LT_WINDOWS)
    // Windows implementation
    std::wstring wpath = ltlib::utf8_to_utf16(path);
    HANDLE handle = CreateFileW(wpath.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
        return nullptr;
    }
    auto uf = std::make_unique<LockedFile>();
    uf->handle_ = handle;
    uf->mutex_ = std::make_unique<FileMutex>(handle);
    return uf;
#elif defined(LT_LINUX)
    // Linux implementation
#else
#error unsupported platform
#endif
}

LockedFile::~LockedFile()
{
#if defined(LT_WINDOWS)
    // Windows implementation
    if (handle_ != INVALID_HANDLE_VALUE) {
        CloseHandle(handle_);
        handle_ = nullptr;
    }
#elif defined(LT_LINUX)
    // Linux implementation
#else
#error unsupported platform
#endif
}

std::string LockedFile::read_str()
{
    std::lock_guard<FileMutex> lock { *mutex_ };
#if defined(LT_WINDOWS)
    // Windows implementation
    constexpr DWORD _4MB = 4 * 1024 * 1024;
    LARGE_INTEGER _size {};
    BOOL ret = GetFileSizeEx(handle_, &_size);
    if (ret != TRUE) {
        return "";
    }
    DWORD size = _size.QuadPart;
    DWORD read_size;
    if (size > _4MB) {
        return "";
    }
    std::string str(' ', size);
    SetFilePointer(handle_, 0, nullptr, FILE_BEGIN);
    ret = ReadFile(handle_, str.data(), size, &read_size, nullptr);
    if (ret != TRUE) {
        return "";
    }
    if (size != read_size) {
        // error
        return "";
    }
    return str;
#elif defined(LT_LINUX)
    // Linux implementation
#else
#error unsupported platform
#endif
}

void LockedFile::write_str(const std::string& str)
{
    std::lock_guard<FileMutex> lock { *mutex_ };
#if defined(LT_WINDOWS)
    // Windows implementation
    LARGE_INTEGER _size {};
    BOOL ret = GetFileSizeEx(handle_, &_size);
    if (ret != TRUE) {
        return;
    }
    DWORD written_size = 0;
    SetFilePointer(handle_, 0, nullptr, FILE_BEGIN);
    ret = WriteFile(handle_, str.data(), _size.QuadPart, &written_size, nullptr);
#elif defined(LT_LINUX)
    // Linux implementation
#else
#error unsupported platform
#endif
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
        std::unique_ptr<LockedFile> file;
        if (ltlib::steady_now_ms() - last_read_time_ms_ >= k5ms) {
            file = LockedFile::open(filepath_);
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
        std::unique_ptr<LockedFile> file;
        if (ltlib::steady_now_ms() - last_read_time_ms_ >= k5ms) {
            file = LockedFile::open(filepath_);
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