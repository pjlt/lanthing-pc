#include <gtest/gtest.h>

#include <ltlib/transform.h>

namespace {

void expectRectEq(const ltlib::Rect& actual, const ltlib::Rect& expected) {
    EXPECT_EQ(actual.x, expected.x);
    EXPECT_EQ(actual.y, expected.y);
    EXPECT_EQ(actual.w, expected.w);
    EXPECT_EQ(actual.h, expected.h);
}

TEST(TransformTest, FitFourByThreeIntoSixteenByNineContainer) {
    const ltlib::Rect outer{0, 0, 1920, 1080};
    const ltlib::Rect innerOriginal{0, 0, 1024, 768};

    const ltlib::Rect actual = ltlib::calcMaxInnerRect(outer, innerOriginal);

    expectRectEq(actual, ltlib::Rect{240, 0, 1440, 1080});
}

TEST(TransformTest, FitSixteenByNineIntoFourByThreeContainer) {
    const ltlib::Rect outer{0, 0, 1024, 768};
    const ltlib::Rect innerOriginal{0, 0, 1920, 1080};

    const ltlib::Rect actual = ltlib::calcMaxInnerRect(outer, innerOriginal);

    expectRectEq(actual, ltlib::Rect{0, 96, 1024, 576});
}

TEST(TransformTest, FitLandscapeIntoPortraitContainer) {
    const ltlib::Rect outer{0, 0, 1080, 1920};
    const ltlib::Rect innerOriginal{0, 0, 1920, 1080};

    const ltlib::Rect actual = ltlib::calcMaxInnerRect(outer, innerOriginal);

    expectRectEq(actual, ltlib::Rect{0, 656, 1080, 607});
}

TEST(TransformTest, HandlesZeroOuterWidthBoundary) {
    const ltlib::Rect outer{0, 0, 0, 1080};
    const ltlib::Rect innerOriginal{0, 0, 1920, 1080};

    const ltlib::Rect actual = ltlib::calcMaxInnerRect(outer, innerOriginal);

    expectRectEq(actual, ltlib::Rect{0, 540, 0, 0});
}

TEST(TransformTest, HandlesZeroInnerWidthBoundary) {
    const ltlib::Rect outer{0, 0, 1920, 1080};
    const ltlib::Rect innerOriginal{0, 0, 0, 1080};

    const ltlib::Rect actual = ltlib::calcMaxInnerRect(outer, innerOriginal);

    expectRectEq(actual, ltlib::Rect{960, 0, 0, 1080});
}

} // namespace