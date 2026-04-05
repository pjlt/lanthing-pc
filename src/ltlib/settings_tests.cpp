#include <gtest/gtest.h>
#include <ltlib/settings.h>
#include <ltlib/times.h>

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

static const char* DBName = "SettingsSqlite.db";

class SettingsSqliteTest : public testing::Test {
protected:
    void SetUp() override {
        settings_ =
            ltlib::Settings::createWithPathForTest(ltlib::Settings::Storage::Sqlite, DBName);
    }
    void TearDown() override { remove(DBName); }

    std::unique_ptr<ltlib::Settings> settings_;
};

TEST_F(SettingsSqliteTest, Creation) {
    EXPECT_NE(settings_, nullptr); // 怎么退出整个TestSuit？
    EXPECT_EQ(settings_->type(), ltlib::Settings::Storage::Sqlite);
}

TEST_F(SettingsSqliteTest, GetEmptyValue) {
    EXPECT_EQ(settings_->getBoolean("non_exists_key"), std::nullopt);
    EXPECT_EQ(settings_->getInteger("non_exists_key"), std::nullopt);
    EXPECT_EQ(settings_->getString("non_exists_key"), std::nullopt);
}

TEST_F(SettingsSqliteTest, SetValue) {
    settings_->setBoolean("bool_value_true", true);
    settings_->setBoolean("bool_value_false", false);
    EXPECT_EQ(settings_->getBoolean("bool_value_true"), true);
    EXPECT_EQ(settings_->getBoolean("bool_value_false"), false);

    settings_->setInteger("int_value_112234", 112234);
    settings_->setInteger("int_value_0", 0);
    settings_->setInteger("int_value_-3456", -3456);
    EXPECT_EQ(settings_->getInteger("int_value_112234"), 112234);
    EXPECT_EQ(settings_->getInteger("int_value_0"), 0);
    EXPECT_EQ(settings_->getInteger("int_value_-3456"), -3456);

    settings_->setString("str_value_hello", "hello");
    settings_->setString("str_value_1234", "1234");
    settings_->setString("str_value_empty", "");
    EXPECT_EQ(settings_->getString("str_value_hello"), "hello");
    EXPECT_EQ(settings_->getString("str_value_1234"), "1234");
    EXPECT_EQ(settings_->getString("str_value_empty"), "");
}

TEST_F(SettingsSqliteTest, UpdateValue) {
    settings_->setBoolean("bool_key", true);
    settings_->setInteger("int_key", 1234);
    settings_->setString("str_key", "some string");

    settings_->setBoolean("bool_key", false);
    settings_->setInteger("int_key", 5678);
    settings_->setString("str_key", "another string");

    EXPECT_EQ(settings_->getBoolean("bool_key"), false);
    EXPECT_EQ(settings_->getInteger("int_key"), 5678);
    EXPECT_EQ(settings_->getString("str_key"), "another string");
}

TEST_F(SettingsSqliteTest, UpdateTime) {
    // UpdateTime是有Bug的，时间戳不更新，待修复
    auto now = ltlib::utc_now_ms() / 1000;
    settings_->setInteger("int_key", 1);
    auto updated_at = settings_->getUpdateTime("int_key");
    ASSERT_TRUE(updated_at.has_value());
    EXPECT_TRUE(updated_at.value() >= now - 1 && updated_at.value() <= now + 1);
}

TEST_F(SettingsSqliteTest, KeysStartWithAndDeleteKey) {
    settings_->setInteger("pref.alpha", 1);
    settings_->setInteger("pref.beta", 2);
    settings_->setInteger("other.gamma", 3);

    auto keys = settings_->getKeysStartWith("pref.");
    std::sort(keys.begin(), keys.end());
    ASSERT_EQ(keys.size(), 2U);
    EXPECT_EQ(keys[0], "pref.alpha");
    EXPECT_EQ(keys[1], "pref.beta");

    settings_->deleteKey("pref.alpha");
    EXPECT_EQ(settings_->getInteger("pref.alpha"), std::nullopt);
    EXPECT_EQ(settings_->getInteger("pref.beta"), 2);
}

TEST_F(SettingsSqliteTest, EmptyKeyIsIgnored) {
    settings_->setInteger("", 123);
    settings_->setBoolean("", true);
    settings_->setString("", "value");

    EXPECT_EQ(settings_->getInteger(""), std::nullopt);
    EXPECT_EQ(settings_->getBoolean(""), std::nullopt);
    EXPECT_EQ(settings_->getString(""), std::nullopt);
    EXPECT_TRUE(settings_->getKeysStartWith("").empty());
}

TEST_F(SettingsSqliteTest, MismatchedValueTypeReturnsNullopt) {
    settings_->setString("mixed", "text");
    settings_->setInteger("int_only", 42);

    EXPECT_EQ(settings_->getInteger("mixed"), std::nullopt);
    EXPECT_EQ(settings_->getBoolean("int_only"), std::nullopt);
}

TEST_F(SettingsSqliteTest, ConcurrentWritesRemainReadable) {
    constexpr int kThreadCount = 8;
    constexpr int kLoopCount = 100;

    std::vector<std::thread> workers;
    workers.reserve(kThreadCount);

    for (int tid = 0; tid < kThreadCount; ++tid) {
        workers.emplace_back([this, tid]() {
            const std::string key = "concurrent." + std::to_string(tid);
            for (int i = 0; i < kLoopCount; ++i) {
                settings_->setInteger(key, tid * 1000 + i);
            }
        });
    }

    for (auto& worker : workers) {
        worker.join();
    }

    for (int tid = 0; tid < kThreadCount; ++tid) {
        const std::string key = "concurrent." + std::to_string(tid);
        const auto value = settings_->getInteger(key);
        ASSERT_TRUE(value.has_value());
        EXPECT_EQ(value.value(), tid * 1000 + (kLoopCount - 1));
    }
}

TEST(SettingsSqliteStandaloneTest, CorruptedFileNeedsRecreateToRecover) {
    static const char* kCorruptedDbName = "SettingsSqliteCorrupted.db";

    {
        std::ofstream out(kCorruptedDbName, std::ios::binary | std::ios::trunc);
        ASSERT_TRUE(out.is_open());
        out << "not a sqlite database";
    }

    auto broken = ltlib::Settings::createWithPathForTest(ltlib::Settings::Storage::Sqlite,
                                                         kCorruptedDbName);
    EXPECT_EQ(broken, nullptr);

    std::remove(kCorruptedDbName);

    auto recovered = ltlib::Settings::createWithPathForTest(ltlib::Settings::Storage::Sqlite,
                                                            kCorruptedDbName);
    ASSERT_NE(recovered, nullptr);
    recovered->setString("ok", "1");
    EXPECT_EQ(recovered->getString("ok"), "1");

    std::remove(kCorruptedDbName);
}