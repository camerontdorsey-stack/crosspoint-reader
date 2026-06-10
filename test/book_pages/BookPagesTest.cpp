#include <gtest/gtest.h>

#include "src/activities/reader/BookPages.h"

using BookPages::estimateBookPage;
using BookPages::estimateBookPageCount;

TEST(BookPageCount, ExtrapolatesFromChapterSpan) {
  // 20-page chapter covering 10% of the book -> ~200 pages total.
  EXPECT_EQ(estimateBookPageCount(20, 0.1f), 200);
  // Chapter that is the whole book.
  EXPECT_EQ(estimateBookPageCount(37, 1.0f), 37);
}

TEST(BookPageCount, RejectsDegenerateInputs) {
  EXPECT_EQ(estimateBookPageCount(0, 0.5f), -1);
  EXPECT_EQ(estimateBookPageCount(-3, 0.5f), -1);
  EXPECT_EQ(estimateBookPageCount(10, 0.0f), -1);
  EXPECT_EQ(estimateBookPageCount(10, -0.2f), -1);
  EXPECT_EQ(estimateBookPageCount(10, 1.5f), -1);
}

TEST(BookPageCount, RejectsExplodedEstimates) {
  // A 1-page dedication spanning a sliver of the book must not yield a junk total.
  EXPECT_EQ(estimateBookPageCount(1, 0.00001f), -1);
}

TEST(BookPageCount, NeverBelowChapterPages) {
  // Rounding can't shrink the book below the chapter we're holding.
  EXPECT_GE(estimateBookPageCount(10, 0.999f), 10);
}

TEST(BookPage, MapsProgressToOneBasedPage) {
  EXPECT_EQ(estimateBookPage(0.0f, 200), 1);    // clamped up from 0
  EXPECT_EQ(estimateBookPage(50.0f, 200), 100);
  EXPECT_EQ(estimateBookPage(100.0f, 200), 200);
}

TEST(BookPage, ClampsOutOfRangeProgress) {
  EXPECT_EQ(estimateBookPage(-5.0f, 200), 1);
  EXPECT_EQ(estimateBookPage(120.0f, 200), 200);
}

TEST(BookPage, RejectsDegenerateTotal) {
  EXPECT_EQ(estimateBookPage(50.0f, 0), -1);
  EXPECT_EQ(estimateBookPage(50.0f, -1), -1);
}

TEST(BookPage, ConsistentPairNearChapterBoundaries) {
  // Display pair must stay within [1, total] across the whole progress range.
  const int total = estimateBookPageCount(25, 0.08f);
  ASSERT_GT(total, 0);
  for (float pct = 0.0f; pct <= 100.0f; pct += 2.5f) {
    const int page = estimateBookPage(pct, total);
    EXPECT_GE(page, 1);
    EXPECT_LE(page, total);
  }
}
