#include <gtest/gtest.h>

#include <vector>

#include "src/activities/boot_sleep/BigDigits.h"

struct Rect {
  int x, y, w, h;
};

static std::vector<Rect> draw(const std::string& text, int height) {
  std::vector<Rect> rects;
  BigDigits::drawText(text, 0, 0, height, 10, [&](int x, int y, int w, int h) { rects.push_back({x, y, w, h}); });
  return rects;
}

TEST(BigDigits, EveryDigitDrawsSomething) {
  for (char c = '0'; c <= '9'; c++) {
    EXPECT_FALSE(draw(std::string(1, c), 160).empty()) << c;
  }
  EXPECT_FALSE(draw("%", 160).empty());
}

TEST(BigDigits, SegmentCountsMatchSevenSegment) {
  EXPECT_EQ(draw("8", 160).size(), 7u);
  EXPECT_EQ(draw("1", 160).size(), 2u);
  EXPECT_EQ(draw("0", 160).size(), 6u);
}

TEST(BigDigits, AllRectsWithinBounds) {
  const int h = 160;
  for (const auto& r : draw("45%", h)) {
    EXPECT_GE(r.x, 0);
    EXPECT_GE(r.y, 0);
    EXPECT_LE(r.y + r.h, h + 1);
    EXPECT_GT(r.w, 0);
    EXPECT_GT(r.h, 0);
  }
}

TEST(BigDigits, TextWidthMatchesDrawnExtent) {
  const int h = 160;
  const std::string text = "100%";
  int maxRight = 0;
  for (const auto& r : draw(text, h)) maxRight = std::max(maxRight, r.x + r.w);
  EXPECT_EQ(BigDigits::textWidth(text, h, 10), maxRight);
}
