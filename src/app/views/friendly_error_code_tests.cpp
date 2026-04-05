#include <gtest/gtest.h>

#include <app/views/friendly_error_code.h>

#include <ltproto/error_code.pb.h>

namespace {

QString friendlySuffix(const QString& message) {
    constexpr auto kSeparator = "\n    ";
    const int idx = message.indexOf(kSeparator);
    if (idx < 0) {
        return message;
    }

    return message.mid(idx + 5);
}

TEST(FriendlyErrorCodeTest, KnownErrorCodeHasSpecificFriendlySuffix) {
    const auto message = errorCode2FriendlyMessage(ltproto::ErrorCode::InvalidParameter);

    EXPECT_TRUE(message.startsWith(QString("Error code: %1").arg(ltproto::ErrorCode::InvalidParameter)));
    EXPECT_EQ(friendlySuffix(message), QStringLiteral("Invalid parameters"));
}

TEST(FriendlyErrorCodeTest, UnknownCodeFallsBackToUnknownMessage) {
    constexpr int32_t kUnknownCode = 123456789;
    const auto unknown_baseline = errorCode2FriendlyMessage(ltproto::ErrorCode::Unknown);
    const auto message = errorCode2FriendlyMessage(kUnknownCode);

    EXPECT_TRUE(message.startsWith(QString("Error code: %1").arg(kUnknownCode)));
    EXPECT_EQ(friendlySuffix(message), friendlySuffix(unknown_baseline));
}

} // namespace
