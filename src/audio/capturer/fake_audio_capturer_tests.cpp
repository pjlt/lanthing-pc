#include <gtest/gtest.h>

#include <audio/capturer/fake_audio_capturer.h>

TEST(FakeAudioCapturerTest, DefaultParametersAreInitialized) {
    lt::audio::Capturer::Params params;
    params.type = lt::AudioCodecType::PCM;
    params.on_audio_data = [](const std::shared_ptr<google::protobuf::MessageLite>&) {};

    lt::audio::FakeAudioCapturer capturer{params};

    EXPECT_EQ(capturer.bytesPerFrame(), 8U);
    EXPECT_EQ(capturer.framesPerSec(), 48000U);
    EXPECT_EQ(capturer.channels(), 2U);
    EXPECT_EQ(capturer.framesPer10ms(), 480U);
    EXPECT_EQ(capturer.bytesPer10ms(), 3840U);
}

TEST(FakeAudioCapturerTest, InitPlatformSucceedsAndCaptureLoopIsNoop) {
    lt::audio::Capturer::Params params;
    params.type = lt::AudioCodecType::PCM;
    params.on_audio_data = [](const std::shared_ptr<google::protobuf::MessageLite>&) {};

    lt::audio::FakeAudioCapturer capturer{params};

    EXPECT_TRUE(capturer.initPlatform());

    int alive_call_count = 0;
    capturer.captureLoop([&alive_call_count]() { ++alive_call_count; });
    EXPECT_EQ(alive_call_count, 0);
}
