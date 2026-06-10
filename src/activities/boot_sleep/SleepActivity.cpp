#include "SleepActivity.h"

#include <Epub.h>
#include <FsHelpers.h>
#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Txt.h>
#include <Xtc.h>

#include <algorithm>
#include <cstring>
#include <vector>

#include "BigDigits.h"
#include "CrossPointSettings.h"
#include "CrossPointState.h"
#include "ReadingStats.h"
#include "activities/reader/ReaderUtils.h"
#include "activities/reader/ReadingEstimate.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "images/Logo120.h"
#include "images/MoonIcon.h"

void SleepActivity::onEnter() {
  Activity::onEnter();

  const bool renderQuickResume =
      SETTINGS.sleepScreen == CrossPointSettings::SLEEP_SCREEN_MODE::QUICK_RESUME ||
      (fromTimeout &&
       SETTINGS.quickResumeSleepScreen == CrossPointSettings::QUICK_RESUME_SLEEP_SCREEN::QUICK_RESUME_AFTER_TIMEOUT);

  if (renderQuickResume) {
    return renderLastScreenSleepScreen();
  }

  // Show popup with reader orientation only when going to sleep from reader
  if (APP_STATE.lastSleepFromReader) {
    ReaderUtils::applyOrientation(renderer, SETTINGS.orientation);
    GUI.drawPopup(renderer, tr(STR_ENTERING_SLEEP));
    renderer.setOrientation(GfxRenderer::Orientation::Portrait);
  } else {
    GUI.drawPopup(renderer, tr(STR_ENTERING_SLEEP));
  }

  switch (SETTINGS.sleepScreen) {
    case (CrossPointSettings::SLEEP_SCREEN_MODE::BLANK):
      return renderBlankSleepScreen();
    case (CrossPointSettings::SLEEP_SCREEN_MODE::FRONTISPIECE):
      return renderFrontispieceSleepScreen();
    case (CrossPointSettings::SLEEP_SCREEN_MODE::DASHBOARD):
      return renderDashboardSleepScreen();
    case (CrossPointSettings::SLEEP_SCREEN_MODE::CUSTOM):
      return renderCustomSleepScreen();
    case (CrossPointSettings::SLEEP_SCREEN_MODE::COVER):
      return renderCoverSleepScreen();
    case (CrossPointSettings::SLEEP_SCREEN_MODE::COVER_CUSTOM):
      if (APP_STATE.lastSleepFromReader) {
        return renderCoverSleepScreen();
      } else {
        return renderCustomSleepScreen();
      }
    default:
      return renderDefaultSleepScreen();
  }
}

void SleepActivity::renderCustomSleepScreen() const {
  // Check if we have a /.sleep (preferred) or /sleep directory
  const char* sleepDir = nullptr;
  auto dir = Storage.open("/.sleep");

  // Look for sleep.bmp on the root of the sd card to determine if we should
  // render a custom sleep screen instead of the default.
  // This takes priority over the /sleep folder.
  HalFile file;
  if (Storage.openFileForRead("SLP", "/sleep.bmp", file)) {
    Bitmap bitmap(file, true);
    if (bitmap.parseHeaders() == BmpReaderError::Ok) {
      LOG_DBG("SLP", "Loading: /sleep.bmp");
      renderBitmapSleepScreen(bitmap);
      file.close();
      if (dir) dir.close();
      return;
    }
    file.close();
  }

  if (dir && dir.isDirectory()) {
    sleepDir = "/.sleep";
  } else {
    dir = Storage.open("/sleep");
    if (dir && dir.isDirectory()) {
      sleepDir = "/sleep";
    }
  }

  if (sleepDir) {
    std::vector<std::string> files;
    char name[500];
    // collect all valid BMP files
    for (auto dirFile = dir.openNextFile(); dirFile; dirFile = dir.openNextFile()) {
      if (dirFile.isDirectory()) {
        dirFile.close();
        continue;
      }
      dirFile.getName(name, sizeof(name));
      auto filename = std::string(name);
      if (filename[0] == '.') {
        dirFile.close();
        continue;
      }

      if (!FsHelpers::hasBmpExtension(filename)) {
        LOG_DBG("SLP", "Skipping non-.bmp file name: %s", name);
        dirFile.close();
        continue;
      }
      Bitmap bitmap(dirFile);
      if (bitmap.parseHeaders() != BmpReaderError::Ok) {
        LOG_DBG("SLP", "Skipping invalid BMP file: %s", name);
        dirFile.close();
        continue;
      }
      files.emplace_back(filename);
      dirFile.close();
    }
    const auto numFiles = files.size();
    if (numFiles > 0) {
      // Pick a random wallpaper, excluding recently shown ones.
      // Window: up to SLEEP_RECENT_COUNT entries, capped at numFiles-1.
      const uint16_t fileCount = static_cast<uint16_t>(std::min(numFiles, static_cast<size_t>(UINT16_MAX)));
      const uint8_t window =
          static_cast<uint8_t>(std::min(static_cast<size_t>(APP_STATE.recentSleepFill), numFiles - 1));
      auto randomFileIndex = static_cast<uint16_t>(random(fileCount));
      for (uint8_t attempt = 0; attempt < 20 && APP_STATE.isRecentSleep(randomFileIndex, window); attempt++) {
        randomFileIndex = static_cast<uint16_t>(random(fileCount));
      }
      APP_STATE.pushRecentSleep(randomFileIndex);
      APP_STATE.saveToFile();
      const auto filename = std::string(sleepDir) + "/" + files[randomFileIndex];
      HalFile randFile;
      if (Storage.openFileForRead("SLP", filename, randFile)) {
        LOG_DBG("SLP", "Randomly loading: %s/%s", sleepDir, files[randomFileIndex].c_str());
        delay(100);
        Bitmap bitmap(randFile, true);
        if (bitmap.parseHeaders() == BmpReaderError::Ok) {
          renderBitmapSleepScreen(bitmap);
          randFile.close();
          dir.close();
          return;
        }
        randFile.close();
      }
    }
  }
  if (dir) dir.close();

  renderDefaultSleepScreen();
}

void SleepActivity::renderDefaultSleepScreen() const {
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  renderer.clearScreen();
  renderer.drawImage(Logo120, (pageWidth - 120) / 2, (pageHeight - 120) / 2, 120, 120);
  renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 + 70, tr(STR_CROSSPOINT), true, EpdFontFamily::BOLD);
  renderer.drawCenteredText(SMALL_FONT_ID, pageHeight / 2 + 95, tr(STR_SLEEPING));

  // Make sleep screen dark unless light is selected in settings
  if (SETTINGS.sleepScreen != CrossPointSettings::SLEEP_SCREEN_MODE::LIGHT) {
    renderer.invertScreen();
  }

  renderer.displayBuffer(HalDisplay::HALF_REFRESH);
}

void SleepActivity::renderBitmapSleepScreen(const Bitmap& bitmap) const {
  int x, y;
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();
  float cropX = 0, cropY = 0;

  LOG_DBG("SLP", "bitmap %d x %d, screen %d x %d", bitmap.getWidth(), bitmap.getHeight(), pageWidth, pageHeight);
  if (bitmap.getWidth() > pageWidth || bitmap.getHeight() > pageHeight) {
    // image will scale, make sure placement is right
    float ratio = static_cast<float>(bitmap.getWidth()) / static_cast<float>(bitmap.getHeight());
    const float screenRatio = static_cast<float>(pageWidth) / static_cast<float>(pageHeight);

    LOG_DBG("SLP", "bitmap ratio: %f, screen ratio: %f", ratio, screenRatio);
    if (ratio > screenRatio) {
      // image wider than viewport ratio, scaled down image needs to be centered vertically
      if (SETTINGS.sleepScreenCoverMode == CrossPointSettings::SLEEP_SCREEN_COVER_MODE::CROP) {
        cropX = 1.0f - (screenRatio / ratio);
        LOG_DBG("SLP", "Cropping bitmap x: %f", cropX);
        ratio = (1.0f - cropX) * static_cast<float>(bitmap.getWidth()) / static_cast<float>(bitmap.getHeight());
      }
      x = 0;
      y = std::round((static_cast<float>(pageHeight) - static_cast<float>(pageWidth) / ratio) / 2);
      LOG_DBG("SLP", "Centering with ratio %f to y=%d", ratio, y);
    } else {
      // image taller than viewport ratio, scaled down image needs to be centered horizontally
      if (SETTINGS.sleepScreenCoverMode == CrossPointSettings::SLEEP_SCREEN_COVER_MODE::CROP) {
        cropY = 1.0f - (ratio / screenRatio);
        LOG_DBG("SLP", "Cropping bitmap y: %f", cropY);
        ratio = static_cast<float>(bitmap.getWidth()) / ((1.0f - cropY) * static_cast<float>(bitmap.getHeight()));
      }
      x = std::round((static_cast<float>(pageWidth) - static_cast<float>(pageHeight) * ratio) / 2);
      y = 0;
      LOG_DBG("SLP", "Centering with ratio %f to x=%d", ratio, x);
    }
  } else {
    // center the image
    x = (pageWidth - bitmap.getWidth()) / 2;
    y = (pageHeight - bitmap.getHeight()) / 2;
  }

  // Resolve the optional "now reading" pane content before any heavy rendering.
  loadSleepInfoIfEnabled();

  LOG_DBG("SLP", "drawing to %d x %d", x, y);
  renderer.clearScreen();

  const bool hasGreyscale = bitmap.hasGreyscale() &&
                            SETTINGS.sleepScreenCoverFilter == CrossPointSettings::SLEEP_SCREEN_COVER_FILTER::NO_FILTER;

  renderer.drawBitmap(bitmap, x, y, pageWidth, pageHeight, cropX, cropY);

  if (SETTINGS.sleepScreenCoverFilter == CrossPointSettings::SLEEP_SCREEN_COVER_FILTER::INVERTED_BLACK_AND_WHITE) {
    renderer.invertScreen();
  }

  // Overlay the pane after the filter so it isn't inverted along with the wallpaper.
  drawSleepInfoPane();

  renderer.displayBuffer(HalDisplay::HALF_REFRESH);

  if (hasGreyscale) {
    // For grayscale wallpapers the visible frame is the gray buffer, so the pane must be baked into
    // both gray planes (drawn once per pass) to appear on the final image.
    bitmap.rewindToData();
    renderer.clearScreen(0x00);
    renderer.setRenderMode(GfxRenderer::GRAYSCALE_LSB);
    renderer.drawBitmap(bitmap, x, y, pageWidth, pageHeight, cropX, cropY);
    drawSleepInfoPane();
    renderer.copyGrayscaleLsbBuffers();

    bitmap.rewindToData();
    renderer.clearScreen(0x00);
    renderer.setRenderMode(GfxRenderer::GRAYSCALE_MSB);
    renderer.drawBitmap(bitmap, x, y, pageWidth, pageHeight, cropX, cropY);
    drawSleepInfoPane();
    renderer.copyGrayscaleMsbBuffers();

    renderer.displayGrayBuffer();
    renderer.setRenderMode(GfxRenderer::BW);
  }
}

void SleepActivity::renderCoverSleepScreen() const {
  void (SleepActivity::*renderNoCoverSleepScreen)() const;
  switch (SETTINGS.sleepScreen) {
    case (CrossPointSettings::SLEEP_SCREEN_MODE::COVER_CUSTOM):
      renderNoCoverSleepScreen = &SleepActivity::renderCustomSleepScreen;
      break;
    default:
      renderNoCoverSleepScreen = &SleepActivity::renderDefaultSleepScreen;
      break;
  }

  if (APP_STATE.openEpubPath.empty()) {
    return (this->*renderNoCoverSleepScreen)();
  }

  std::string coverBmpPath;
  bool cropped = SETTINGS.sleepScreenCoverMode == CrossPointSettings::SLEEP_SCREEN_COVER_MODE::CROP;

  // Check if the current book is XTC, TXT, or EPUB
  if (FsHelpers::hasXtcExtension(APP_STATE.openEpubPath)) {
    // Handle XTC file
    Xtc lastXtc(APP_STATE.openEpubPath, "/.crosspoint");
    if (!lastXtc.load()) {
      LOG_ERR("SLP", "Failed to load last XTC");
      return (this->*renderNoCoverSleepScreen)();
    }

    if (!lastXtc.generateCoverBmp()) {
      LOG_ERR("SLP", "Failed to generate XTC cover bmp");
      return (this->*renderNoCoverSleepScreen)();
    }

    coverBmpPath = lastXtc.getCoverBmpPath();
  } else if (FsHelpers::hasTxtExtension(APP_STATE.openEpubPath)) {
    // Handle TXT file - looks for cover image in the same folder
    Txt lastTxt(APP_STATE.openEpubPath, "/.crosspoint");
    if (!lastTxt.load()) {
      LOG_ERR("SLP", "Failed to load last TXT");
      return (this->*renderNoCoverSleepScreen)();
    }

    if (!lastTxt.generateCoverBmp()) {
      LOG_ERR("SLP", "No cover image found for TXT file");
      return (this->*renderNoCoverSleepScreen)();
    }

    coverBmpPath = lastTxt.getCoverBmpPath();
  } else if (FsHelpers::hasEpubExtension(APP_STATE.openEpubPath)) {
    // Handle EPUB file
    Epub lastEpub(APP_STATE.openEpubPath, "/.crosspoint");
    // Skip loading css since we only need metadata here
    if (!lastEpub.load(true, true)) {
      LOG_ERR("SLP", "Failed to load last epub");
      return (this->*renderNoCoverSleepScreen)();
    }

    if (!lastEpub.generateCoverBmp(cropped)) {
      LOG_ERR("SLP", "Failed to generate cover bmp");
      return (this->*renderNoCoverSleepScreen)();
    }

    coverBmpPath = lastEpub.getCoverBmpPath(cropped);
  } else {
    return (this->*renderNoCoverSleepScreen)();
  }

  HalFile file;
  if (Storage.openFileForRead("SLP", coverBmpPath, file)) {
    Bitmap bitmap(file);
    if (bitmap.parseHeaders() == BmpReaderError::Ok) {
      LOG_DBG("SLP", "Rendering sleep cover: %s", coverBmpPath.c_str());
      renderBitmapSleepScreen(bitmap);
      return;
    }
  }

  return (this->*renderNoCoverSleepScreen)();
}

void SleepActivity::renderLastScreenSleepScreen() const {
  const auto pageHeight = renderer.getScreenHeight();
  renderer.drawImage(MoonIcon, 0, pageHeight - MOONICON_HEIGHT, MOONICON_WIDTH, MOONICON_HEIGHT);
  renderer.displayBuffer(HalDisplay::HALF_REFRESH);
}

void SleepActivity::renderBlankSleepScreen() const {
  renderer.clearScreen();
  renderer.displayBuffer(HalDisplay::HALF_REFRESH);
}

void SleepActivity::loadSleepInfoIfEnabled(const bool force) const {
  infoTitle.clear();
  infoAuthor.clear();

  // Reading stats are independent of the open book; the typographic sleep modes show them even
  // with no book in flight.
  ReadingStats::Stats stats;
  if (ReadingStats::load(stats)) {
    infoSessionMin = static_cast<long>(stats.sessionMs / 60000UL);
    infoLifetimeMin = static_cast<long>(stats.lifetimeMs / 60000ULL);
    infoSessionPages = stats.sessionPages;
    infoLifetimePages = stats.lifetimePages;
  }

  if ((!force && SETTINGS.sleepInfoPane == CrossPointSettings::SLEEP_INFO_PANE_OFF) || APP_STATE.openEpubPath.empty()) {
    return;
  }
  // v1 reads metadata from EPUBs only; TXT/XTC simply show no pane.
  if (!FsHelpers::hasEpubExtension(APP_STATE.openEpubPath)) {
    return;
  }
  Epub epub(APP_STATE.openEpubPath, "/.crosspoint");
  if (!epub.load(true, true)) {  // metadata only — skip CSS and spine bodies
    LOG_DBG("SLP", "Info pane: epub metadata load failed");
    return;
  }
  infoTitle = epub.getTitle();

  // Pages/time remaining, derived from the saved progress cache plus the persisted reading speed.
  infoPagesLeft = -1;
  infoMinutesLeft = -1;

  // Read saved progress. Layout (see EpubReaderActivity::onEnter): [0,1]=spine, [2,3]=current page,
  // [4,5]=chapter page count.
  uint16_t spineIndex = 0, curPage = 0, chapterTotal = 0;
  bool haveProgress = false;
  HalFile pf;
  if (Storage.openFileForRead("SLP", epub.getCachePath() + "/progress.bin", pf)) {
    uint8_t data[6];
    if (pf.read(data, 6) == 6) {
      spineIndex = static_cast<uint16_t>(data[0] + (data[1] << 8));
      curPage = static_cast<uint16_t>(data[2] + (data[3] << 8));
      chapterTotal = static_cast<uint16_t>(data[4] + (data[5] << 8));
      if (curPage == UINT16_MAX) curPage = 0;  // resume sentinel, treat as start
      haveProgress = true;
    }
  }

  // Second line: the current chapter's title (when selected and resolvable), otherwise the author.
  if (SETTINGS.sleepInfoSecondary == CrossPointSettings::SLEEP_INFO_SECONDARY_CHAPTER && haveProgress) {
    const int tocIndex = epub.getTocIndexForSpineIndex(spineIndex);
    if (tocIndex != -1) {
      infoAuthor = epub.getTocItem(tocIndex).title;
    }
  }
  if (infoAuthor.empty()) {
    infoAuthor = epub.getAuthor();
  }

  if (chapterTotal == 0) {
    return;  // no usable page metrics; pane still shows title + second line
  }

  if (haveProgress && chapterTotal > 0) {
    const float chapFrac = static_cast<float>(curPage + 1) / static_cast<float>(chapterTotal);
    infoProgressPct = static_cast<int>(epub.calculateProgress(spineIndex, chapFrac) * 100.0f + 0.5f);
    if (infoProgressPct < 0) infoProgressPct = 0;
    if (infoProgressPct > 100) infoProgressPct = 100;
  }

  const int curPage1 = static_cast<int>(curPage) + 1;
  const float chapterLeft = ReadingEstimate::chapterPagesLeft(curPage1, chapterTotal);
  float pagesLeft = chapterLeft;
  if (SETTINGS.sleepInfoReference == CrossPointSettings::SLEEP_INFO_REF_BOOK) {
    // Extrapolate whole-book pages from this chapter's byte-span share (same model as the reader).
    const float chapFrac = static_cast<float>(curPage1) / static_cast<float>(chapterTotal);
    const float bookProgressPct = epub.calculateProgress(spineIndex, chapFrac) * 100.0f;
    const float span = epub.calculateProgress(spineIndex, 1.0f) - epub.calculateProgress(spineIndex, 0.0f);
    pagesLeft = ReadingEstimate::bookPagesLeft(chapterLeft, chapterTotal, span, bookProgressPct);
  }
  infoPagesLeft = static_cast<int>(pagesLeft + 0.5f);

  // Reading speed (global), persisted by the reader on each progress save, enables the time estimate.
  HalFile rs;
  if (Storage.openFileForRead("SLP", "/.crosspoint/readspeed.bin", rs)) {
    uint8_t buf[6];
    if (rs.read(buf, 6) == 6) {
      float msPerPage = 0.0f;
      memcpy(&msPerPage, buf, sizeof(float));
      const uint16_t samples = static_cast<uint16_t>(buf[4] + (buf[5] << 8));
      if (samples >= ReadingEstimate::MIN_SAMPLES && msPerPage > 0.0f) {
        infoMinutesLeft = ReadingEstimate::minutesLeft(msPerPage, pagesLeft);
      }
    }
  }
}

void SleepActivity::drawSleepInfoPane() const {
  if (infoTitle.empty()) {
    return;
  }
  // Draw in the user's reading orientation: wallpaper art is authored for how the device is
  // physically held, which is what the reader orientation setting captures. Restored below.
  ReaderUtils::applyOrientation(renderer, SETTINGS.orientation);
  const int screenW = renderer.getScreenWidth();
  const int screenH = renderer.getScreenHeight();

  // "Ticket stub": a full-width opaque band, separated from the wallpaper by a 2px rule.
  // Position (top/bottom) follows the existing pane-position setting.
  constexpr int titleFont = UI_10_FONT_ID;
  constexpr int bodyFont = SMALL_FONT_ID;
  constexpr int padX = 20;
  constexpr int padY = 12;
  constexpr int lineGap = 6;
  constexpr int ruleH = 2;

  const int titleH = renderer.getLineHeight(titleFont);
  const int bodyH = renderer.getLineHeight(bodyFont);
  const bool haveStats = infoLifetimePages > 0;
  const int rows = haveStats ? 3 : 2;
  const int bandH = 2 * padY + titleH + bodyH + (haveStats ? bodyH : 0) + (rows - 1) * lineGap + ruleH;
  const bool top = SETTINGS.sleepInfoPanePosition == CrossPointSettings::SLEEP_INFO_PANE_TOP;
  const int bandY = top ? 0 : screenH - bandH;

  renderer.fillRect(0, bandY, screenW, bandH, false);  // white band
  // Rule on the inner edge (between band and wallpaper).
  renderer.fillRect(0, top ? bandY + bandH - ruleH : bandY, screenW, ruleH, true);

  int y = bandY + padY + (top ? 0 : ruleH);
  const int maxW = screenW - 2 * padX;

  // Row 1: title left (bold), book percent right.
  std::string pctStr = (infoProgressPct >= 0) ? std::to_string(infoProgressPct) + "%" : "";
  const int pctW = pctStr.empty() ? 0 : renderer.getTextWidth(titleFont, pctStr.c_str(), EpdFontFamily::BOLD);
  const std::string title =
      renderer.truncatedText(titleFont, infoTitle.c_str(), maxW - pctW - (pctW ? padX : 0), EpdFontFamily::BOLD);
  renderer.drawText(titleFont, padX, y, title.c_str(), true, EpdFontFamily::BOLD);
  if (!pctStr.empty()) {
    renderer.drawText(titleFont, screenW - padX - pctW, y, pctStr.c_str(), true, EpdFontFamily::BOLD);
  }
  y += titleH + lineGap;

  // Row 2: author/chapter left, pages/time-left right (per the existing content settings).
  std::string metric;
  const bool refBook = SETTINGS.sleepInfoReference == CrossPointSettings::SLEEP_INFO_REF_BOOK;
  const uint8_t content = SETTINGS.sleepInfoContent;
  const bool wantPages =
      content == CrossPointSettings::SLEEP_INFO_PAGES || content == CrossPointSettings::SLEEP_INFO_BOTH;
  const bool wantTime =
      content == CrossPointSettings::SLEEP_INFO_TIME || content == CrossPointSettings::SLEEP_INFO_BOTH;
  if (wantPages && infoPagesLeft >= 0) {
    const StrId suffix = (infoPagesLeft == 1)
                             ? (refBook ? StrId::STR_PAGE_LEFT_IN_BOOK : StrId::STR_PAGE_LEFT_IN_CHAPTER)
                             : (refBook ? StrId::STR_PAGES_LEFT_IN_BOOK : StrId::STR_PAGES_LEFT_IN_CHAPTER);
    metric = std::to_string(infoPagesLeft) + " " + I18N.get(suffix);
  }
  if (wantTime && infoMinutesLeft >= 0) {
    const StrId suffix = refBook ? StrId::STR_MIN_LEFT_IN_BOOK : StrId::STR_MIN_LEFT_IN_CHAPTER;
    if (!metric.empty()) metric += "  ";
    metric += "~" + std::to_string(infoMinutesLeft) + " " + I18N.get(suffix);
  }
  const int metricW = metric.empty() ? 0 : renderer.getTextWidth(bodyFont, metric.c_str());
  const std::string author =
      renderer.truncatedText(bodyFont, infoAuthor.c_str(), maxW - metricW - (metricW ? padX : 0));
  renderer.drawText(bodyFont, padX, y, author.c_str());
  if (!metric.empty()) {
    renderer.drawText(bodyFont, screenW - padX - metricW, y, metric.c_str());
  }
  y += bodyH + lineGap;

  // Row 3: reading stats (session left, all-time right). Omitted until any stats exist.
  if (haveStats) {
    std::string session = ReadingStats::formatDuration(infoSessionMin < 0 ? 0 : infoSessionMin) + " · " +
                          std::to_string(infoSessionPages) + " pages today";
    std::string lifetime = ReadingStats::formatDuration(infoLifetimeMin) + " all time";
    const int lifeW = renderer.getTextWidth(bodyFont, lifetime.c_str());
    session = renderer.truncatedText(bodyFont, session.c_str(), maxW - lifeW - padX);
    renderer.drawText(bodyFont, padX, y, session.c_str());
    renderer.drawText(bodyFont, screenW - padX - lifeW, y, lifetime.c_str());
  }
  renderer.setOrientation(GfxRenderer::Orientation::Portrait);
}

namespace {
// Shared helpers for the typographic sleep modes.
void drawCenteredText(GfxRenderer& renderer, const int fontId, const int y, const std::string& text,
                      const EpdFontFamily::Style style = EpdFontFamily::REGULAR) {
  const int w = renderer.getTextWidth(fontId, text.c_str(), style);
  renderer.drawText(fontId, (renderer.getScreenWidth() - w) / 2, y, text.c_str(), true, style);
}
}  // namespace

void SleepActivity::renderFrontispieceSleepScreen() const {
  loadSleepInfoIfEnabled(/*force=*/true);
  ReaderUtils::applyOrientation(renderer, SETTINGS.orientation);
  renderer.clearScreen();
  const int screenW = renderer.getScreenWidth();
  const int screenH = renderer.getScreenHeight();

  // Double-rule frame, fine-press style.
  renderer.drawRect(16, 16, screenW - 32, screenH - 32, 2, true);
  renderer.drawRect(24, 24, screenW - 48, screenH - 48, 1, true);

  const int contentW = screenW - 120;
  int y = screenH / 5;

  if (!infoTitle.empty()) {
    const std::string title =
        renderer.truncatedText(NOTOSERIF_18_FONT_ID, infoTitle.c_str(), contentW, EpdFontFamily::BOLD);
    drawCenteredText(renderer, NOTOSERIF_18_FONT_ID, y, title, EpdFontFamily::BOLD);
    y += renderer.getLineHeight(NOTOSERIF_18_FONT_ID) + 16;
    if (!infoAuthor.empty()) {
      const std::string author = renderer.truncatedText(NOTOSERIF_14_FONT_ID, infoAuthor.c_str(), contentW);
      drawCenteredText(renderer, NOTOSERIF_14_FONT_ID, y, author);
      y += renderer.getLineHeight(NOTOSERIF_14_FONT_ID) + 28;
    }
  } else {
    drawCenteredText(renderer, NOTOSERIF_18_FONT_ID, y, "CrossPoint", EpdFontFamily::BOLD);
    y += renderer.getLineHeight(NOTOSERIF_18_FONT_ID) + 28;
  }

  // Ornament.
  renderer.fillRect(screenW / 2 - 5, y + 5, 10, 10, true);
  y += 40;

  if (infoProgressPct >= 0) {
    drawCenteredText(renderer, SMALL_FONT_ID, y, std::to_string(infoProgressPct) + "% COMPLETE");
    y += renderer.getLineHeight(SMALL_FONT_ID) + 14;
    // Thin framed progress bar.
    const int barW = contentW;
    const int barX = (screenW - barW) / 2;
    renderer.drawRect(barX, y, barW, 10, 1, true);
    renderer.fillRect(barX, y, barW * infoProgressPct / 100, 10, true);
    y += 28;
  }
  if (infoMinutesLeft >= 0) {
    drawCenteredText(renderer, SMALL_FONT_ID, y, "~" + ReadingStats::formatDuration(infoMinutesLeft) + " left");
    y += renderer.getLineHeight(SMALL_FONT_ID) + 16;
  }

  // Stats block anchored above the footer.
  int statsY = screenH - 150;
  renderer.fillRect(60, statsY, screenW - 120, 1, true);
  statsY += 16;
  if (infoLifetimePages > 0) {
    drawCenteredText(renderer, SMALL_FONT_ID, statsY,
                     "Today " + ReadingStats::formatDuration(infoSessionMin < 0 ? 0 : infoSessionMin) + " · " +
                         std::to_string(infoSessionPages) + " pages");
    statsY += renderer.getLineHeight(SMALL_FONT_ID) + 8;
    drawCenteredText(renderer, SMALL_FONT_ID, statsY,
                     "All time " + ReadingStats::formatDuration(infoLifetimeMin) + " · " +
                         std::to_string(infoLifetimePages) + " pages");
    statsY += renderer.getLineHeight(SMALL_FONT_ID) + 8;
  }
  drawCenteredText(renderer, UI_10_FONT_ID, screenH - 70, "CrossPoint", EpdFontFamily::BOLD);

  renderer.setOrientation(GfxRenderer::Orientation::Portrait);
  renderer.displayBuffer();
}

void SleepActivity::renderDashboardSleepScreen() const {
  loadSleepInfoIfEnabled(/*force=*/true);
  ReaderUtils::applyOrientation(renderer, SETTINGS.orientation);
  renderer.clearScreen();
  const int screenW = renderer.getScreenWidth();
  const int screenH = renderer.getScreenHeight();

  renderer.drawRect(16, 16, screenW - 32, screenH - 32, 2, true);

  const int contentW = screenW - 120;
  int y = 70;

  if (!infoTitle.empty()) {
    const std::string title = renderer.truncatedText(UI_10_FONT_ID, infoTitle.c_str(), contentW, EpdFontFamily::BOLD);
    drawCenteredText(renderer, UI_10_FONT_ID, y, title, EpdFontFamily::BOLD);
    y += renderer.getLineHeight(UI_10_FONT_ID) + 6;
    if (!infoAuthor.empty()) {
      drawCenteredText(renderer, SMALL_FONT_ID, y, renderer.truncatedText(SMALL_FONT_ID, infoAuthor.c_str(), contentW));
    }
  } else {
    drawCenteredText(renderer, UI_10_FONT_ID, y, "CrossPoint", EpdFontFamily::BOLD);
  }

  // Centerpiece: oversized percent in block numerals.
  const int pct = infoProgressPct >= 0 ? infoProgressPct : 0;
  const std::string pctText = std::to_string(pct) + "%";
  constexpr int digitH = 160;
  constexpr int digitGap = 18;
  const int bigW = BigDigits::textWidth(pctText, digitH, digitGap);
  const int bigX = (screenW - bigW) / 2;
  const int bigY = screenH / 2 - digitH / 2 - 40;
  BigDigits::drawText(
      pctText, bigX, bigY, digitH, digitGap,
      [this](const int x, const int yy, const int w, const int h) { renderer.fillRect(x, yy, w, h, true); });
  drawCenteredText(renderer, SMALL_FONT_ID, bigY + digitH + 18, "COMPLETE");

  int rowY = bigY + digitH + 60;
  // Framed progress bar.
  const int barW = contentW;
  const int barX = (screenW - barW) / 2;
  renderer.drawRect(barX, rowY, barW, 12, 1, true);
  renderer.fillRect(barX, rowY, barW * pct / 100, 12, true);
  rowY += 34;
  if (infoMinutesLeft >= 0) {
    drawCenteredText(renderer, SMALL_FONT_ID, rowY, "~" + ReadingStats::formatDuration(infoMinutesLeft) + " left");
    rowY += renderer.getLineHeight(SMALL_FONT_ID) + 10;
  }

  if (infoLifetimePages > 0) {
    const int statsY = screenH - 130;
    renderer.fillRect(60, statsY, screenW - 120, 1, true);
    drawCenteredText(renderer, SMALL_FONT_ID, statsY + 14,
                     "Today " + ReadingStats::formatDuration(infoSessionMin < 0 ? 0 : infoSessionMin) + " · " +
                         std::to_string(infoSessionPages) + " pages   |   All time " +
                         ReadingStats::formatDuration(infoLifetimeMin));
  }
  drawCenteredText(renderer, UI_10_FONT_ID, screenH - 70, "CrossPoint", EpdFontFamily::BOLD);

  renderer.setOrientation(GfxRenderer::Orientation::Portrait);
  renderer.displayBuffer();
}
