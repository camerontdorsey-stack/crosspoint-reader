#pragma once

#include <algorithm>
#include <cstdint>

// Pure, side-effect-free math for the status-bar "time left to read" estimate.
// Kept header-only and dependency-free so it can be unit-tested on the host
// (see test/reading_estimate/) without pulling in the firmware/render stack.
namespace ReadingEstimate {

// Page-turn gaps outside this window are not counted as reading: faster is a skim,
// slower means the reader was idle/away.
inline constexpr unsigned long MIN_SAMPLE_MS = 1500UL;
inline constexpr unsigned long MAX_SAMPLE_MS = 300000UL;  // 5 minutes
// Weight applied to the newest sample in the exponential moving average.
inline constexpr float EMA_ALPHA = 0.3f;
// Don't surface an estimate until the average has settled over a few turns.
inline constexpr uint16_t MIN_SAMPLES = 3;
// Collect this many in-content samples, then seed the average from their median. Using the median of a
// small warm-up window means one or two odd pages at the very start (a sparse chapter-title page, a long
// pause) can't anchor the whole estimate to a bad baseline.
inline constexpr uint8_t WARMUP_SAMPLES = 5;

// Median of up to WARMUP_SAMPLES values (sorts a local copy; does not mutate the input).
inline float median(const float* values, int count) {
  if (count <= 0) return 0.0f;
  if (count > WARMUP_SAMPLES) count = WARMUP_SAMPLES;
  float tmp[WARMUP_SAMPLES];
  for (int i = 0; i < count; i++) tmp[i] = values[i];
  std::sort(tmp, tmp + count);
  return tmp[count / 2];
}

// A dwell outside [average / FACTOR, average * FACTOR] is treated as an outlier and dropped:
// too long = an interruption (distracted, didn't lock the device); too short = a sparse or
// mis-rendered page with little text. Either one would skew the reading-speed average.
inline constexpr float OUTLIER_FACTOR = 2.5f;

// Whether a page-dwell duration should count toward the reading-speed average.
inline bool isValidSample(unsigned long deltaMs) { return deltaMs >= MIN_SAMPLE_MS && deltaMs <= MAX_SAMPLE_MS; }

// Whether a (valid-window) dwell is an outlier versus the current average — symmetric, so abnormally
// long AND abnormally short pages are both rejected. Only kicks in once the average has settled, so
// early samples that legitimately set the baseline aren't rejected.
inline bool isOutlier(unsigned long deltaMs, float currentEma, uint16_t sampleCount) {
  if (sampleCount < MIN_SAMPLES || currentEma <= 0.0f) return false;
  const float d = static_cast<float>(deltaMs);
  return d > OUTLIER_FACTOR * currentEma || d < currentEma / OUTLIER_FACTOR;
}

// Fold a new ms-per-page sample into the running average. The first sample seeds the average outright.
inline float updateEma(float prevEma, uint16_t priorSampleCount, unsigned long deltaMs) {
  const float d = static_cast<float>(deltaMs);
  return (priorSampleCount == 0) ? d : EMA_ALPHA * d + (1.0f - EMA_ALPHA) * prevEma;
}

// Pages remaining in the current chapter. currentPage is 1-based (the page being read).
inline float chapterPagesLeft(int currentPage1Based, int pageCount) {
  return std::max(0.0f, static_cast<float>(pageCount) - static_cast<float>(currentPage1Based));
}

// Pages remaining in the whole book, extrapolated from how big a slice of the book the current
// chapter occupies (chapterSpan, a 0..1 fraction by byte size). Falls back to the chapter figure
// when the span is unknown, and never returns less than the chapter's own remaining pages.
inline float bookPagesLeft(float chapterPagesLeftVal, int pageCount, float chapterSpan, float bookProgressPct) {
  if (chapterSpan <= 0.0001f || pageCount <= 0) return chapterPagesLeftVal;
  const float totalBookPages = static_cast<float>(pageCount) / chapterSpan;
  return std::max(chapterPagesLeftVal, totalBookPages * (1.0f - bookProgressPct / 100.0f));
}

// Convert a ms-per-page rate and a page count into whole minutes (rounded).
inline int minutesLeft(float msPerPage, float pagesLeft) {
  if (msPerPage <= 0.0f || pagesLeft <= 0.0f) return 0;
  return static_cast<int>(pagesLeft * msPerPage / 60000.0f + 0.5f);
}

}  // namespace ReadingEstimate
