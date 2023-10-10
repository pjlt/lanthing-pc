#include <gtest/gtest.h>
#include <ltlib/settings.h>

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