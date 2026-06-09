#include <gtest/gtest.h>

#include "ReadingEstimate.h"

using namespace ReadingEstimate;

TEST(ReadingEstimate, ValidSampleWindow) {
  EXPECT_FALSE(isValidSample(MIN_SAMPLE_MS - 1));
  EXPECT_TRUE(isValidSample(MIN_SAMPLE_MS));
  EXPECT_TRUE(isValidSample(30000));
  EXPECT_TRUE(isValidSample(MAX_SAMPLE_MS));
  EXPECT_FALSE(isValidSample(MAX_SAMPLE_MS + 1));
}

TEST(ReadingEstimate, FirstSampleSeedsEma) { EXPECT_FLOAT_EQ(updateEma(0.0f, 0, 20000), 20000.0f); }

TEST(ReadingEstimate, EmaBlendsTowardNewSamples) {
  float ema = updateEma(0.0f, 0, 20000);  // seed -> 20000
  ema = updateEma(ema, 1, 40000);         // 0.3*40000 + 0.7*20000 = 26000
  EXPECT_FLOAT_EQ(ema, 26000.0f);
  ema = updateEma(ema, 2, 40000);  // 0.3*40000 + 0.7*26000 = 30200
  EXPECT_FLOAT_EQ(ema, 30200.0f);
}

TEST(ReadingEstimate, OutlierRejectsLongAndShortOnlyAfterAverageSettles) {
  // Before MIN_SAMPLES, nothing is an outlier (early samples set the baseline).
  EXPECT_FALSE(isOutlier(999999, 20000.0f, 0));
  EXPECT_FALSE(isOutlier(999999, 20000.0f, MIN_SAMPLES - 1));
  // Once settled, a dwell beyond FACTOR x average is rejected (interruption)...
  EXPECT_TRUE(isOutlier(static_cast<unsigned long>(20000 * OUTLIER_FACTOR) + 1, 20000.0f, MIN_SAMPLES));
  // ...and a dwell below average / FACTOR is rejected (sparse/glitch page)...
  EXPECT_TRUE(isOutlier(static_cast<unsigned long>(20000 / OUTLIER_FACTOR) - 1, 20000.0f, MIN_SAMPLES));
  // ...but dwells within the band are kept.
  EXPECT_FALSE(isOutlier(25000, 20000.0f, MIN_SAMPLES));  // a bit slow
  EXPECT_FALSE(isOutlier(40000, 20000.0f, MIN_SAMPLES));  // exactly 2x, under 2.5x factor
  EXPECT_FALSE(isOutlier(10000, 20000.0f, MIN_SAMPLES));  // half average, within band
}

TEST(ReadingEstimate, MedianSeedIsRobustToOutliers) {
  float withLongOutlier[5] = {20000, 21000, 19000, 100000, 18000};
  EXPECT_FLOAT_EQ(median(withLongOutlier, 5), 20000.0f);  // sorted middle of {18,19,20,21,100}k
  float withShortOutlier[5] = {30000, 1600, 29000, 31000, 28000};
  EXPECT_FLOAT_EQ(median(withShortOutlier, 5), 29000.0f);  // sorted middle of {1.6,28,29,30,31}k
  float threeVals[3] = {10000, 50000, 12000};
  EXPECT_FLOAT_EQ(median(threeVals, 3), 12000.0f);
  EXPECT_FLOAT_EQ(median(threeVals, 0), 0.0f);  // empty
}

TEST(ReadingEstimate, ChapterPagesLeftClampsAtZero) {
  EXPECT_FLOAT_EQ(chapterPagesLeft(1, 10), 9.0f);
  EXPECT_FLOAT_EQ(chapterPagesLeft(10, 10), 0.0f);
  EXPECT_FLOAT_EQ(chapterPagesLeft(11, 10), 0.0f);  // never negative
}

TEST(ReadingEstimate, MinutesLeftRounds) {
  EXPECT_EQ(minutesLeft(30000.0f, 9.0f), 5);  // 4.5 min -> 5
  EXPECT_EQ(minutesLeft(30000.0f, 4.0f), 2);  // 2.0 min -> 2
  EXPECT_EQ(minutesLeft(0.0f, 9.0f), 0);      // no speed yet
  EXPECT_EQ(minutesLeft(30000.0f, 0.0f), 0);  // nothing left
}

TEST(ReadingEstimate, BookPagesLeftExtrapolatesFromSpan) {
  // Chapter is 10% of the book (span=0.1), 20 pages -> ~200 total book pages.
  // At 50% progress ~100 remain.
  const float chapLeft = chapterPagesLeft(15, 20);  // 5
  EXPECT_NEAR(bookPagesLeft(chapLeft, 20, 0.1f, 50.0f), 100.0f, 0.01f);
}

TEST(ReadingEstimate, BookPagesLeftFallsBackWhenSpanUnknown) {
  const float chapLeft = chapterPagesLeft(15, 20);  // 5
  EXPECT_FLOAT_EQ(bookPagesLeft(chapLeft, 20, 0.0f, 50.0f), chapLeft);
}

TEST(ReadingEstimate, BookPagesLeftNeverBelowChapterRemaining) {
  // Near the end of the book but the current chapter still has pages left.
  const float chapLeft = chapterPagesLeft(15, 20);  // 5
  EXPECT_GE(bookPagesLeft(chapLeft, 20, 0.5f, 99.0f), chapLeft);
}
