/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2023 Zhennan Tu <zhennan.tu@gmail.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#if defined(LT_WINDOWS)
#include <Windows.h>
#elif defined(LT_LINUX)
#include <fcntl.h>
#else
#error unsupported platform
#endif
#include <ltlib/settings.h>

#include <inttypes.h>

#include <array>
#include <filesystem>
#include <sstream>

#include <sqlite3.h>
#include <toml++/toml.h>

#include <ltlib/logging.h>
#include <ltlib/strings.h>
#include <ltlib/system.h>
#include <ltlib/times.h>

namespace {

constexpr uint32_t k5ms = 5;

#if defined(LT_WINDOWS)
// Windows implementation
class FileMutex {
public:
    FileMutex(HANDLE handle)
        : handle_{handle} {}
    void lock() {
        BOOL ret;
        DWORD size_low, size_high;
        OVERLAPPED ol;
        size_low = GetFileSize(handle_, &size_high);
        ::memset(&ol, 0, sizeof(OVERLAPPED));
        ret = LockFileEx(handle_, LOCKFILE_EXCLUSIVE_LOCK, 0, size_low, size_high, &ol);
    }
    void unlock() {
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
class FileMutex {
public:
    FileMutex(int fd)
        : fd_{fd} {}
    void lock() {}
    void unlock() {}

private:
    int fd_;
};
#else
#error unsupported platform
#endif

class LockedFile {
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

std::unique_ptr<LockedFile> LockedFile::open(const std::string& path) {
#if defined(LT_WINDOWS)
    // Windows implementation
    std::wstring wpath = ltlib::utf8To16(path);
    HANDLE handle =
        CreateFileW(wpath.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                    nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
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

LockedFile::~LockedFile() {
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

std::string LockedFile::read_str() {
    std::lock_guard<FileMutex> lock{*mutex_};
#if defined(LT_WINDOWS)
    // Windows implementation
    constexpr DWORD _4MB = 4 * 1024 * 1024;
    LARGE_INTEGER _size{};
    BOOL ret = GetFileSizeEx(handle_, &_size);
    if (ret != TRUE) {
        return "";
    }
    DWORD size = static_cast<DWORD>(_size.QuadPart);
    DWORD read_size;
    if (size > _4MB) {
        return "";
    }
    std::string str(size, ' ');
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

void LockedFile::write_str(const std::string& str) {
    std::lock_guard<FileMutex> lock{*mutex_};
#if defined(LT_WINDOWS)
    // Windows implementation
    DWORD written_size = 0;
    SetFilePointer(handle_, 0, nullptr, FILE_BEGIN);
    BOOL ret =
        WriteFile(handle_, str.data(), static_cast<DWORD>(str.size()), &written_size, nullptr);
    (void)ret; // FIXME: error handling??
    ret = SetEndOfFile(handle_);
    (void)ret;
#elif defined(LT_LINUX)
    // Linux implementation
#else
#error unsupported platform
#endif
}

} // namespace

namespace ltlib {

class SettingsToml : public Settings {
public:
    SettingsToml(const std::string& path);
    ~SettingsToml() override = default;
    bool init() override;
    Storage type() const override { return Storage::Toml; }
    void setBoolean(const std::string& key, bool value) override;
    auto getBoolean(const std::string& key) -> std::optional<bool> override;
    void setInteger(const std::string& key, int64_t value) override;
    auto getInteger(const std::string& key) -> std::optional<int64_t> override;
    void setString(const std::string& key, const std::string& value) override;
    auto getString(const std::string& key) -> std::optional<std::string> override;

private:
    template <typename VType> void set_value(const std::string& key, VType value) {
        std::lock_guard<std::mutex> lk{mutex_};
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
        ss << toml::toml_formatter{settings};
        text_ = ss.str();
        file->write_str(text_);
    }

    template <typename VType> std::optional<VType> get_value(const std::string& key) {
        std::lock_guard<std::mutex> lk{mutex_};
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

SettingsToml::SettingsToml(const std::string& path)
    : filepath_{path} {}

bool SettingsToml::init() {
    auto file = std::fstream{filepath_, std::ios::in | std::ios::out | std::ios::app};
    if (file.fail()) {
        return false;
    }
    std::stringstream ss;
    ss << file.rdbuf();
    text_ = ss.str();
    last_read_time_ms_ = ltlib::steady_now_ms();
    return true;
}

void SettingsToml::setBoolean(const std::string& key, bool value) {
    set_value(key, value);
}

auto SettingsToml::getBoolean(const std::string& key) -> std::optional<bool> {
    auto value = get_value<bool>(key);
    return value;
}

void SettingsToml::setInteger(const std::string& key, int64_t value) {
    set_value(key, value);
}

auto SettingsToml::getInteger(const std::string& key) -> std::optional<int64_t> {
    auto value = get_value<int64_t>(key);
    return value;
}

void SettingsToml::setString(const std::string& key, const std::string& value) {
    set_value(key, value);
}

auto SettingsToml::getString(const std::string& key) -> std::optional<std::string> {
    auto value = get_value<std::string>(key);
    return value;
}

//*********************↑↑↑↑↑↑SettingsToml↑↑↑↑↑↑*******************************
// 1. 不应该把配置用文本直接暴露给普通用户
// 2. 将跨进程锁的实现交给9千万行测试代码的sqlite，而不是手搓玩具
//*********************↓↓↓↓↓SettingsSqlite↓↓↓↓↓*******************************

// TODO:
static bool validateStr(const std::string& str) {
    if (str.empty()) {
        return false;
    }
    return true;
}

// 只是存储本地，手拼SQL不会爆炸吧？
// 算了， TODO: 校验key和value
class SettingsSqlite : public Settings {
public:
    SettingsSqlite(const std::string& path);
    ~SettingsSqlite() override;
    bool init() override;
    Storage type() const override { return Storage::Sqlite; }
    void setBoolean(const std::string& key, bool value) override;
    auto getBoolean(const std::string& key) -> std::optional<bool> override;
    void setInteger(const std::string& key, int64_t value) override;
    auto getInteger(const std::string& key) -> std::optional<int64_t> override;
    void setString(const std::string& key, const std::string& value) override;
    auto getString(const std::string& key) -> std::optional<std::string> override;

private:
    sqlite3* db_ = nullptr;
    std::string filepath_;
};

SettingsSqlite::SettingsSqlite(const std::string& path)
    : filepath_{path} {}

SettingsSqlite::~SettingsSqlite() {
    if (db_ != nullptr) {
        sqlite3_close(db_);
    }
}

bool SettingsSqlite::init() {
    const char* kCreateTableSQL = R"(
CREATE TABLE IF NOT EXISTS kv_settings(
	"id"	        INTEGER PRIMARY KEY,
	"created_at"	DATETIME NOT NULL DEFAULT (datetime(CURRENT_TIMESTAMP, 'localtime')), 
	"updated_at"	DATETIME NOT NULL DEFAULT (strftime('%Y-%m-%d %H:%M:%f', 'now', 'localtime')),
	"name"          TEXT UNIQUE NOT NULL,
	"bool_val"      INTEGER,
	"int_val"       INTEGER,
	"str_val"       TEXT,
	"blob_val"      BLOB
);)";
    int ret = sqlite3_open(filepath_.c_str(), &db_);
    if (ret != SQLITE_OK) {
        LOG(ERR) << "sqlite3_open failed with " << ret;
        return false;
    }
    char* errmsg = nullptr;
    ret = sqlite3_exec(db_, kCreateTableSQL, nullptr, nullptr, &errmsg);
    if (ret != SQLITE_OK) {
        LOG(ERR) << "Create kv_settings failed: " << errmsg;
        sqlite3_free(errmsg);
        return false;
    }
    return true;
}

void SettingsSqlite::setBoolean(const std::string& key, bool value) {
    if (!validateStr(key)) {
        return;
    }
    const char sql[] = R"(INSERT OR IGNORE INTO kv_settings (name, bool_val) VALUES ('%s', %d);
UPDATE kv_settings SET bool_val = %d WHERE name='%s';
)";
    std::array<char, 512> buff = {0};
    if (key.size() * 2 + sizeof(sql) > buff.size()) {
        LOG(ERR) << "setBoolean failed, key too long";
        return;
    }
    snprintf(buff.data(), buff.size(), sql, key.c_str(), value ? 1 : 0, value ? 1 : 0, key.c_str());
    char* errmsg = nullptr;
    int ret = sqlite3_exec(db_, buff.data(), nullptr, nullptr, &errmsg);
    if (ret != SQLITE_OK) {
        LOGF(ERR, "setBoolean '%s' failed, sqlite3_exec: %s", key.c_str(), errmsg);
        sqlite3_free(errmsg);
    }
}

auto SettingsSqlite::getBoolean(const std::string& key) -> std::optional<bool> {
    if (!validateStr(key)) {
        return std::nullopt;
    }
    const char sql[] = R"(SELECT bool_val FROM kv_settings WHERE name='%s';)";
    std::array<char, 128> buff = {0};
    if (key.size() + sizeof(sql) > buff.size()) {
        LOG(ERR) << "getBoolean failed, key too long";
        return std::nullopt;
    }
    snprintf(buff.data(), buff.size(), sql, key.c_str());
    char* errmsg = nullptr;
    std::optional<bool> result;
    int ret = sqlite3_exec(
        db_, buff.data(),
        [](void* op, int argc, char** argv, char**) -> int { // 回调返回值有什么用?
            std::optional<bool>* result = reinterpret_cast<std::optional<bool>*>(op);
            if (argc != 1 || argv[0] == nullptr) {
                LOG(ERR) << "SELECT bool_val failed";
                return 0;
            }
            int val = std::atoi(argv[0]);
            (*result) = val != 0;
            return 0;
        },
        &result, &errmsg);
    if (ret != SQLITE_OK) {
        LOGF(ERR, "getBoolean '%s' failed, sqlite3_exec: %s", key.c_str(), errmsg);
        sqlite3_free(errmsg);
    }
    return result;
}

void SettingsSqlite::setInteger(const std::string& key, int64_t value) {
    if (!validateStr(key)) {
        return;
    }
    const char sql[] = "INSERT OR IGNORE INTO kv_settings (name, int_val) VALUES ('%s', %" PRId64
                       "); UPDATE kv_settings SET bool_val = %" PRId64 " WHERE name = '%s';";
    std::array<char, 512> buff = {0};
    if (key.size() * 2 + sizeof(sql) > buff.size()) {
        LOG(ERR) << "setInteger failed, key too long";
        return;
    }
    snprintf(buff.data(), buff.size(), sql, key.c_str(), value, value, key.c_str());
    char* errmsg = nullptr;
    int ret = sqlite3_exec(db_, buff.data(), nullptr, nullptr, &errmsg);
    if (ret != SQLITE_OK) {
        LOGF(ERR, "setInteger '%s' failed, sqlite3_exec: %s", key.c_str(), errmsg);
        sqlite3_free(errmsg);
    }
}

auto SettingsSqlite::getInteger(const std::string& key) -> std::optional<int64_t> {
    if (!validateStr(key)) {
        return std::nullopt;
    }
    const char sql[] = R"(SELECT int_val FROM kv_settings WHERE name='%s';)";
    std::array<char, 128> buff = {0};
    if (key.size() + sizeof(sql) > buff.size()) {
        LOG(ERR) << "getInteger failed, key too long";
        return std::nullopt;
    }
    snprintf(buff.data(), buff.size(), sql, key.c_str());
    char* errmsg = nullptr;
    std::optional<int64_t> result;
    int ret = sqlite3_exec(
        db_, buff.data(),
        [](void* op, int argc, char** argv, char**) -> int { // 回调返回值有什么用?
            std::optional<int64_t>* result = reinterpret_cast<std::optional<int64_t>*>(op);
            if (argc != 1 || argv[0] == nullptr) {
                LOG(ERR) << "SELECT int_val failed";
                return 0;
            }
            int64_t val = std::atoll(argv[0]);
            (*result) = val;
            return 0;
        },
        &result, &errmsg);
    if (ret != SQLITE_OK) {
        LOGF(ERR, "getInteger '%s' failed, sqlite3_exec: %s", key.c_str(), errmsg);
        sqlite3_free(errmsg);
    }
    return result;
}

void SettingsSqlite::setString(const std::string& key, const std::string& value) {
    if (!validateStr(key)) {
        return;
    }
    // 允许空字符串
    if (!value.empty() && !validateStr(value)) {
        return;
    }
    const char sql[] = R"(INSERT OR IGNORE INTO kv_settings (name, str_val) VALUES ('%s', '%s');
UPDATE kv_settings SET str_val='%s' WHERE name='%s';
)";
    std::array<char, 512> buff = {0};
    if (key.size() * 2 + value.size() * 2 + sizeof(sql) > buff.size()) {
        LOG(ERR) << "setString failed, key too long";
        return;
    }
    snprintf(buff.data(), buff.size(), sql, key.c_str(), value.c_str(), value.c_str(), key.c_str());
    char* errmsg = nullptr;
    int ret = sqlite3_exec(db_, buff.data(), nullptr, nullptr, &errmsg);
    if (ret != SQLITE_OK) {
        LOGF(ERR, "setString '%s' failed, sqlite3_exec: %s", key.c_str(), errmsg);
        sqlite3_free(errmsg);
    }
}

auto SettingsSqlite::getString(const std::string& key) -> std::optional<std::string> {
    if (!validateStr(key)) {
        return std::nullopt;
    }
    const char sql[] = R"(SELECT str_val FROM kv_settings WHERE name='%s';)";
    std::array<char, 128> buff = {0};
    if (key.size() + sizeof(sql) > buff.size()) {
        LOG(ERR) << "getString failed, key too long";
        return std::nullopt;
    }
    snprintf(buff.data(), buff.size(), sql, key.c_str());
    char* errmsg = nullptr;
    std::optional<std::string> result;
    int ret = sqlite3_exec(
        db_, buff.data(),
        [](void* op, int argc, char** argv, char**) -> int { // 回调返回值有什么用?
            std::optional<std::string>* result = reinterpret_cast<std::optional<std::string>*>(op);
            if (argc != 1 || argv[0] == nullptr) {
                LOG(ERR) << "SELECT str_val failed";
                return 0;
            }
            std::string val = argv[0];
            (*result) = val;
            return 0;
        },
        &result, &errmsg);
    if (ret != SQLITE_OK) {
        LOGF(ERR, "getString '%s' failed, sqlite3_exec: %s", key.c_str(), errmsg);
        sqlite3_free(errmsg);
    }
    return result;
}

//****************************************************************************

std::unique_ptr<Settings> Settings::create(Storage type) {
    std::string appdatapath = ltlib::getAppdataPath(ltlib::isRunAsService());
    if (appdatapath.empty()) {
        return nullptr;
    }
    std::filesystem::path filepath = appdatapath;
    std::string filename = type == Storage::Toml ? "settings.toml" : "settings.db";
    filepath = filepath / "lanthing" / filename;
    return createWithPathForTest(type, filepath.string());
}

std::unique_ptr<Settings> Settings::createWithPathForTest(Storage type, const std::string& path) {
    std::unique_ptr<Settings> settings;
    switch (type) {
    case Settings::Storage::Toml:
        settings = std::make_unique<SettingsToml>(path);
        break;
    case Settings::Storage::Sqlite:
        settings = std::make_unique<SettingsSqlite>(path);
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