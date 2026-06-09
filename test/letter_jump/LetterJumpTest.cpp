#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "LetterJump.h"

using LetterJump::jump;

// "apple, avocado, banana, cherry, cherry2, date"
static const std::vector<std::string> kList = {"apple", "avocado", "banana", "cherry", "cherry2", "date"};

TEST(LetterJump, EmptyList) {
  std::vector<std::string> empty;
  EXPECT_EQ(jump(empty, 0, true), 0u);
  EXPECT_EQ(jump(empty, 0, false), 0u);
}

TEST(LetterJump, ForwardSkipsToNextLetterGroup) {
  EXPECT_EQ(jump(kList, 0, true), 2u);  // a-group (0,1) -> 'b' at 2
  EXPECT_EQ(jump(kList, 2, true), 3u);  // 'b' -> 'c' at 3
  EXPECT_EQ(jump(kList, 3, true), 5u);  // 'c' (3,4) -> 'd' at 5
}

TEST(LetterJump, ForwardFromLastGroupSnapsToEnd) {
  EXPECT_EQ(jump(kList, 5, true), 5u);  // 'd' is last group -> stays at end
}

TEST(LetterJump, BackwardSnapsToCurrentLetterFirst) {
  // from avocado (index 1, in 'a' group): backward snaps to start of 'a' (index 0)
  EXPECT_EQ(jump(kList, 1, false), 0u);
  // from cherry2 (index 4, in 'c' group): backward snaps to start of 'c' (index 3)
  EXPECT_EQ(jump(kList, 4, false), 3u);
}

TEST(LetterJump, BackwardFromGroupStartStepsToPreviousGroup) {
  // from 'b' at index 2 (already group start) -> start of 'a' group (index 0)
  EXPECT_EQ(jump(kList, 2, false), 0u);
  // from 'c' at index 3 (group start) -> start of 'b' group (index 2)
  EXPECT_EQ(jump(kList, 3, false), 2u);
}

TEST(LetterJump, BackwardAtTopStaysAtZero) { EXPECT_EQ(jump(kList, 0, false), 0u); }

TEST(LetterJump, CaseInsensitiveGrouping) {
  std::vector<std::string> mixed = {"Apple", "apricot", "Banana"};
  EXPECT_EQ(jump(mixed, 0, true), 2u);   // 'A'/'a' are one group -> 'B' at 2
  EXPECT_EQ(jump(mixed, 2, false), 0u);  // back from 'B' -> start of a-group
}

TEST(LetterJump, DirectoriesGroupByName) {
  // Directory entries end with '/'; grouping uses the first char of the name.
  std::vector<std::string> withDirs = {"art/", "atlas", "books/"};
  EXPECT_EQ(jump(withDirs, 0, true), 2u);  // a-group -> 'b'
}
