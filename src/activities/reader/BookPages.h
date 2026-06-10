#pragma once

#include <cmath>

// Pure math for the status-bar whole-book page counter. The reader lays out one chapter at a
// time, so a true book page count doesn't exist; we extrapolate it from the current chapter's
// laid-out page count and the fraction of the book's bytes that chapter covers. Header-only and
// firmware-free so it can be unit-tested on the host (see test/book_pages/).
namespace BookPages {

// Hard ceiling on the extrapolated count. A chapter covering a sliver of the book (a half-page
// dedication) can explode the estimate; past this it is meaningless, so the caller falls back to
// chapter pages.
constexpr int MAX_BOOK_PAGES = 9999;

// Estimated whole-book page count. chapterPageCount: laid-out pages of the current chapter.
// chapterSpan: fraction of the book's bytes the chapter covers, in (0, 1]. Returns -1 when no
// sane estimate exists.
inline int estimateBookPageCount(const int chapterPageCount, const float chapterSpan) {
  if (chapterPageCount <= 0 || chapterSpan <= 0.0f || chapterSpan > 1.0f) return -1;
  const float total = static_cast<float>(chapterPageCount) / chapterSpan;
  if (total > static_cast<float>(MAX_BOOK_PAGES)) return -1;
  const int rounded = static_cast<int>(total + 0.5f);
  return rounded < chapterPageCount ? chapterPageCount : rounded;
}

// Current page within the whole book, 1-based, derived from overall progress. bookProgressPct is
// 0..100. Clamped to [1, bookPageCount] so the displayed pair never reads "0/N" or "N+1/N".
inline int estimateBookPage(const float bookProgressPct, const int bookPageCount) {
  if (bookPageCount <= 0) return -1;
  const float pct = bookProgressPct < 0.0f ? 0.0f : (bookProgressPct > 100.0f ? 100.0f : bookProgressPct);
  int page = static_cast<int>(std::lround(pct / 100.0f * static_cast<float>(bookPageCount)));
  if (page < 1) page = 1;
  if (page > bookPageCount) page = bookPageCount;
  return page;
}

}  // namespace BookPages
