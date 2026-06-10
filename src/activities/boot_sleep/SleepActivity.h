#pragma once
#include <string>

#include "activities/Activity.h"

class Bitmap;

class SleepActivity final : public Activity {
 public:
  explicit SleepActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, bool fromTimeout = false)
      : Activity("Sleep", renderer, mappedInput), fromTimeout(fromTimeout) {}
  void onEnter() override;

 private:
  void renderDefaultSleepScreen() const;
  void renderCustomSleepScreen() const;
  void renderCoverSleepScreen() const;
  void renderBitmapSleepScreen(const Bitmap& bitmap) const;
  void renderLastScreenSleepScreen() const;
  void renderBlankSleepScreen() const;
  // Full-screen typographic sleep modes (no wallpaper). Frontispiece: fine-press title page.
  // Dashboard: oversized progress numeral as the centerpiece (see BigDigits.h).
  void renderFrontispieceSleepScreen() const;
  void renderDashboardSleepScreen() const;

  // Optional "now reading" info pane overlaid on image-based sleep screens.
  // Populates the cached title/author from the currently-open EPUB when the feature is enabled.
  void loadSleepInfoIfEnabled(bool force = false) const;
  // Draws the frosted info pane using the cached title/author. No-op if there is nothing to show.
  // Honors the renderer's current draw mode so it composes correctly in both BW and grayscale passes.
  void drawSleepInfoPane() const;
  mutable std::string infoTitle;
  mutable std::string infoAuthor;
  // Pages and minutes remaining for the info pane (per the chapter/book reference setting), or -1 if
  // unknown (no cached progress / no persisted reading speed).
  mutable int infoPagesLeft = -1;
  mutable int infoMinutesLeft = -1;
  // Whole-book progress percent (0-100) when cached progress exists, else -1.
  mutable int infoProgressPct = -1;
  // Reading stats (ReadingStats.h); -1 minutes / 0 pages when absent.
  mutable long infoSessionMin = -1;
  mutable long infoLifetimeMin = -1;
  mutable uint32_t infoSessionPages = 0;
  mutable uint32_t infoLifetimePages = 0;

  bool fromTimeout = false;
};
