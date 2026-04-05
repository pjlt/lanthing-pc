#include <gtest/gtest.h>

#include <app/views/components/access_token_validator.h>

namespace {

TEST(AccessTokenValidatorTest, ValidateTrimsTruncatesAndLowercases) {
    AccesstokenValidator validator(nullptr);
    QString input = QStringLiteral("  AbC1239  ");
    int pos = 10;

    const auto state = validator.validate(input, pos);

    EXPECT_EQ(state, QValidator::State::Acceptable);
    EXPECT_EQ(input, QStringLiteral("abc123"));
    EXPECT_EQ(pos, 6);
}

TEST(AccessTokenValidatorTest, ValidateReturnsInvalidForIllegalCharacter) {
    AccesstokenValidator validator(nullptr);
    QString input = QStringLiteral("ab-12");
    int pos = 0;

    const auto state = validator.validate(input, pos);

    EXPECT_EQ(state, QValidator::State::Invalid);
    EXPECT_EQ(input, QStringLiteral("ab-12"));
}

TEST(AccessTokenValidatorTest, ValidateAllowsEmptyInputAfterTrim) {
    AccesstokenValidator validator(nullptr);
    QString input = QStringLiteral("   ");
    int pos = 3;

    const auto state = validator.validate(input, pos);

    EXPECT_EQ(state, QValidator::State::Acceptable);
    EXPECT_TRUE(input.isEmpty());
}

TEST(AccessTokenValidatorTest, FixupUppercasesInput) {
    AccesstokenValidator validator(nullptr);
    QString input = QStringLiteral("ab12cd");

    validator.fixup(input);

    EXPECT_EQ(input, QStringLiteral("AB12CD"));
}

} // namespace
