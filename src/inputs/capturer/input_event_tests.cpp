#include <gtest/gtest.h>

#include <inputs/capturer/input_event.h>

TEST(InputEventTest, KeyboardEventTypeAndPayload) {
    const lt::input::KeyboardEvent source{42, true};
    const lt::input::InputEvent event{source};

    EXPECT_EQ(event.type, lt::input::InputEventType::Keyboard);
    ASSERT_TRUE(std::holds_alternative<lt::input::KeyboardEvent>(event.ev));

    const auto& payload = std::get<lt::input::KeyboardEvent>(event.ev);
    EXPECT_EQ(payload.scan_code, 42);
    EXPECT_TRUE(payload.is_pressed);
}

TEST(InputEventTest, MouseButtonEventTypeAndPayload) {
    const lt::input::MouseButtonEvent source{lt::input::MouseButtonEvent::Button::Right, false, 11,
                                             22, 1280, 720};
    const lt::input::InputEvent event{source};

    EXPECT_EQ(event.type, lt::input::InputEventType::MouseButton);
    ASSERT_TRUE(std::holds_alternative<lt::input::MouseButtonEvent>(event.ev));

    const auto& payload = std::get<lt::input::MouseButtonEvent>(event.ev);
    EXPECT_EQ(payload.button, lt::input::MouseButtonEvent::Button::Right);
    EXPECT_FALSE(payload.is_pressed);
    EXPECT_EQ(payload.x, 11);
    EXPECT_EQ(payload.y, 22);
    EXPECT_EQ(payload.window_width, 1280U);
    EXPECT_EQ(payload.window_height, 720U);
}

TEST(InputEventTest, MouseMoveEventTypeAndPayload) {
    const lt::input::MouseMoveEvent source{100, 80, -2, 3, 1920, 1080};
    const lt::input::InputEvent event{source};

    EXPECT_EQ(event.type, lt::input::InputEventType::MouseMove);
    ASSERT_TRUE(std::holds_alternative<lt::input::MouseMoveEvent>(event.ev));

    const auto& payload = std::get<lt::input::MouseMoveEvent>(event.ev);
    EXPECT_EQ(payload.x, 100);
    EXPECT_EQ(payload.y, 80);
    EXPECT_EQ(payload.delta_x, -2);
    EXPECT_EQ(payload.delta_y, 3);
    EXPECT_EQ(payload.window_width, 1920U);
    EXPECT_EQ(payload.window_height, 1080U);
}

TEST(InputEventTest, MouseWheelEventTypeAndPayload) {
    const lt::input::MouseWheelEvent source{-120};
    const lt::input::InputEvent event{source};

    EXPECT_EQ(event.type, lt::input::InputEventType::MouseWheel);
    ASSERT_TRUE(std::holds_alternative<lt::input::MouseWheelEvent>(event.ev));

    const auto& payload = std::get<lt::input::MouseWheelEvent>(event.ev);
    EXPECT_EQ(payload.amount, -120);
}

TEST(InputEventTest, ControllerAddedRemovedEventTypeAndPayload) {
    const lt::input::ControllerAddedRemovedEvent source{7, true};
    const lt::input::InputEvent event{source};

    EXPECT_EQ(event.type, lt::input::InputEventType::ControllerAddedRemoved);
    ASSERT_TRUE(std::holds_alternative<lt::input::ControllerAddedRemovedEvent>(event.ev));

    const auto& payload = std::get<lt::input::ControllerAddedRemovedEvent>(event.ev);
    EXPECT_EQ(payload.index, 7U);
    EXPECT_TRUE(payload.is_added);
}

TEST(InputEventTest, ControllerButtonEventTypeAndPayload) {
    const lt::input::ControllerButtonEvent source{1, lt::input::ControllerButtonEvent::Button::A,
                                                   false};
    const lt::input::InputEvent event{source};

    EXPECT_EQ(event.type, lt::input::InputEventType::ControllerButton);
    ASSERT_TRUE(std::holds_alternative<lt::input::ControllerButtonEvent>(event.ev));

    const auto& payload = std::get<lt::input::ControllerButtonEvent>(event.ev);
    EXPECT_EQ(payload.index, 1U);
    EXPECT_EQ(payload.button, lt::input::ControllerButtonEvent::Button::A);
    EXPECT_FALSE(payload.is_pressed);
}

TEST(InputEventTest, ControllerAxisEventTypeAndPayload) {
    const lt::input::ControllerAxisEvent source{2, lt::input::ControllerAxisEvent::AxisType::RightThumbY,
                                                -16384};
    const lt::input::InputEvent event{source};

    EXPECT_EQ(event.type, lt::input::InputEventType::ControllerAxis);
    ASSERT_TRUE(std::holds_alternative<lt::input::ControllerAxisEvent>(event.ev));

    const auto& payload = std::get<lt::input::ControllerAxisEvent>(event.ev);
    EXPECT_EQ(payload.index, 2U);
    EXPECT_EQ(payload.axis_type, lt::input::ControllerAxisEvent::AxisType::RightThumbY);
    EXPECT_EQ(payload.value, -16384);
}
