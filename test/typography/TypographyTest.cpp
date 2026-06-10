#include <gtest/gtest.h>

#include "Epub/Typography.h"

using Typography::shouldBreakForOrphan;

// Viewport fits 10 lines of height 80 exactly (800px).
constexpr int LH = 80;
constexpr int VP = 800;

TEST(OrphanControl, BreaksWhenFirstLineLandsInLastSlot) {
  // y=720: line fits (720+80=800) but a second line would not -> orphan.
  EXPECT_TRUE(shouldBreakForOrphan(0, true, 720, LH, VP));
}

TEST(OrphanControl, NoBreakWhenRoomForTwoLines) {
  EXPECT_FALSE(shouldBreakForOrphan(0, true, 640, LH, VP));
  EXPECT_FALSE(shouldBreakForOrphan(0, true, 0, LH, VP));
}

TEST(OrphanControl, NoBreakForSingleLineParagraph) {
  // A one-line paragraph at the bottom is not an orphan.
  EXPECT_FALSE(shouldBreakForOrphan(0, false, 720, LH, VP));
}

TEST(OrphanControl, NoBreakForLaterLines) {
  // Only a paragraph's first line can be an orphan.
  EXPECT_FALSE(shouldBreakForOrphan(1, true, 720, LH, VP));
  EXPECT_FALSE(shouldBreakForOrphan(5, true, 720, LH, VP));
}

TEST(OrphanControl, NoBreakOnEmptyPage) {
  // Tiny viewport: even line 0 with more coming must be placed on an empty page,
  // or a tall-paragraph/short-page combination would defer forever.
  EXPECT_FALSE(shouldBreakForOrphan(0, true, 0, LH, 2 * LH - 1));
}

TEST(OrphanControl, NoBreakWhenLineOverflowsAnyway) {
  // y=760: the line itself doesn't fit; the normal page-break path owns this case.
  EXPECT_FALSE(shouldBreakForOrphan(0, true, 760, LH, VP));
}

TEST(OrphanControl, DegenerateGeometryRejected) {
  EXPECT_FALSE(shouldBreakForOrphan(0, true, 100, 0, VP));
  EXPECT_FALSE(shouldBreakForOrphan(0, true, 100, LH, 0));
}
