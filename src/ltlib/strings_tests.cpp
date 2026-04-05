#include <array>
#include <string>

#include <gtest/gtest.h>

#include <ltlib/strings.h>

namespace {

TEST(StringsTest, Utf8AndUtf16RoundTripAsciiCjkAndEmoji) {
    const std::array<std::string, 4> cases{
        "",
        "hello, world",
        "\xE4\xBD\xA0\xE5\xA5\xBD",
        "\xF0\x9F\x99\x82",
    };

    for (const auto& utf8 : cases) {
        const std::wstring utf16 = ltlib::utf8To16(utf8);
        EXPECT_EQ(ltlib::utf16To8(utf16), utf8);
    }
}

TEST(StringsTest, Utf16AndUtf8RoundTripAsciiCjkAndEmoji) {
    const std::array<std::wstring, 4> cases{
        L"",
        L"hello",
        std::wstring{0x4F60, 0x597D},
        std::wstring{0xD83D, 0xDE42},
    };

    for (const auto& utf16 : cases) {
        const std::string utf8 = ltlib::utf16To8(utf16);
        EXPECT_EQ(ltlib::utf8To16(utf8), utf16);
    }
}

TEST(StringsTest, Base64EncodeDecodeRoundTrip) {
    const std::string raw{"abc\0xyz", 7};

    const std::string encoded = ltlib::base64Encode(raw);
    EXPECT_EQ(encoded, "YWJjAHh5eg==");
    EXPECT_EQ(ltlib::base64Decode(encoded), raw);
}

TEST(StringsTest, Base64DecodeStopsAtInvalidCharacter) {
    const std::string withInvalid = "aGVs#bG8=";

    EXPECT_EQ(ltlib::base64Decode(withInvalid), ltlib::base64Decode("aGVs"));
}

TEST(StringsTest, Base64HandlesEmptyInput) {
    EXPECT_TRUE(ltlib::base64Encode("").empty());
    EXPECT_TRUE(ltlib::base64Decode("").empty());
}

} // namespace