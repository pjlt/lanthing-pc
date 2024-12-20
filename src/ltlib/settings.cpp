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
#else
#include <fcntl.h>
#include <pwd.h>
#include <unistd.h>
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

namespace ltlib {

// SettingsToml已删除

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
    auto getUpdateTime(const std::string& key) -> std::optional<int64_t> override;
    auto getKeysStartWith(const std::string& prefix) -> std::vector<std::string> override;
    void deleteKey(const std::string& key) override;

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
	"created_at"	DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
	"updated_at"	DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
	"name"          TEXT UNIQUE NOT NULL,
	"bool_val"      INTEGER,
	"int_val"       INTEGER,
	"str_val"       TEXT,
	"blob_val"      BLOB
);
CREATE TRIGGER IF NOT EXISTS UpdateTimestamp
    AFTER UPDATE
    ON kv_settings
BEGIN
    UPDATE kv_settings SET updated_at = CURRENT_TIMESTAMP WHERE id=OLD.id;
END;
)";
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
                       "); UPDATE kv_settings SET int_val = %" PRId64 " WHERE name = '%s';";
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

auto SettingsSqlite::getUpdateTime(const std::string& key) -> std::optional<int64_t> {
    if (!validateStr(key)) {
        return std::nullopt;
    }
    const char sql[] = "SELECT strftime('%%s', updated_at) FROM kv_settings WHERE name = '%s';";
    std::array<char, 128> buff = {0};
    if (key.size() + sizeof(sql) > buff.size()) {
        LOG(ERR) << "getString failed, key too long";
        return std::nullopt;
    }
    snprintf(buff.data(), buff.size(), sql, key.c_str());
    char* errmsg = nullptr;
    std::optional<int64_t> result;
    int ret = sqlite3_exec(
        db_, buff.data(),
        [](void* op, int argc, char** argv, char**) -> int {
            std::optional<int64_t>* result = reinterpret_cast<std::optional<int64_t>*>(op);
            if (argc != 1 || argv[0] == nullptr) {
                LOG(ERR) << "SELECT updated_at failed";
                return 0;
            }
            int64_t val = std::atoll(argv[0]);
            (*result) = val;
            return 0;
        },
        &result, &errmsg);
    if (ret != SQLITE_OK) {
        LOGF(ERR, "getUpdateTime '%s' failed, sqlite3_exec: %s", key.c_str(), errmsg);
        sqlite3_free(errmsg);
    }
    return result;
}

auto SettingsSqlite::getKeysStartWith(const std::string& prefix) -> std::vector<std::string> {
    if (!validateStr(prefix)) {
        return {};
    }
    const char sql[] = "SELECT name FROM kv_settings WHERE name LIKE '%s%%';";
    std::array<char, 128> buff = {0};
    if (prefix.size() + sizeof(sql) > buff.size()) {
        LOG(ERR) << "getKeysStartWith failed, prefix too long";
        return {};
    }
    snprintf(buff.data(), buff.size(), sql, prefix.c_str());
    std::vector<std::string> keys;
    char* errmsg = nullptr;
    int ret = sqlite3_exec(
        db_, buff.data(),
        [](void* op, int argc, char** argv, char**) -> int {
            auto result = reinterpret_cast<std::vector<std::string>*>(op);
            for (int i = 0; i < argc; i++) {
                if (argv[i]) {
                    result->push_back(argv[i]);
                }
            }
            return 0;
        },
        &keys, &errmsg);
    if (ret != SQLITE_OK) {
        LOGF(ERR, "getKeysStartWith '%s' failed, sqlite3_exec: %s", prefix.c_str(), errmsg);
        sqlite3_free(errmsg);
    }
    return keys;
}

void SettingsSqlite::deleteKey(const std::string& key) {
    if (!validateStr(key)) {
        return;
    }
    const char sql[] = "DELETE FROM kv_settings WHERE name = '%s';";
    std::array<char, 128> buff = {0};
    if (key.size() + sizeof(sql) > buff.size()) {
        LOG(ERR) << "deleteKey failed, key too long";
        return;
    }
    snprintf(buff.data(), buff.size(), sql, key.c_str());
    char* errmsg = nullptr;
    int ret = sqlite3_exec(
        db_, buff.data(), [](void*, int, char**, char**) -> int { return 0; }, nullptr, &errmsg);
    if (ret != SQLITE_OK) {
        LOGF(ERR, "deleteKey '%s' failed, sqlite3_exec: %s", key.c_str(), errmsg);
        sqlite3_free(errmsg);
    }
}

//****************************************************************************

std::unique_ptr<Settings> Settings::create(Storage type) {
    std::string filename = type == Storage::Toml ? "settings.toml" : "settings.db";
    std::string appdatapath = ltlib::getConfigPath(ltlib::isRunAsService());
    if (appdatapath.empty()) {
        return nullptr;
    }
    std::filesystem::path filepath = appdatapath;
    filepath = filepath / filename;
    return createWithPathForTest(type, filepath.string());
}

std::unique_ptr<Settings> Settings::createWithPathForTest(Storage type, const std::string& path) {
    std::unique_ptr<Settings> settings;
    switch (type) {
    case Settings::Storage::Sqlite:
        settings = std::make_unique<SettingsSqlite>(path);
        break;
    case Settings::Storage::Toml:
        [[fallthrough]];
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
