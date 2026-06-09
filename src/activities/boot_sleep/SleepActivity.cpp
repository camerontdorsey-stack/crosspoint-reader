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

#include "CrossPointSettings.h"
#include "CrossPointState.h"
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

void SleepActivity::loadSleepInfoIfEnabled() const {
  infoTitle.clear();
  infoAuthor.clear();
  if (SETTINGS.sleepInfoPane == CrossPointSettings::SLEEP_INFO_PANE_OFF || APP_STATE.openEpubPath.empty()) {
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
  const int screenW = renderer.getScreenWidth();
  const int screenH = renderer.getScreenHeight();

  constexpr int titleFont = UI_10_FONT_ID;
  constexpr int bodyFont = SMALL_FONT_ID;
  constexpr int padX = 16;        // horizontal text padding inside the pane
  constexpr int padY = 10;        // vertical text padding inside the pane
  constexpr int edgeMargin = 24;  // gap from the screen edges
  constexpr int lineGap = 4;      // gap between stacked lines
  constexpr int radius = 10;      // rounded "glass" corner radius

  const int maxPaneW = screenW - 2 * edgeMargin;
  const int maxTextW = maxPaneW - 2 * padX;

  // Build the up-to-three lines: title (bold), author, "<n> pages left in chapter".
  struct Line {
    std::string text;
    int fontId;
    EpdFontFamily::Style style;
    int width;
    int height;
  };
  std::vector<Line> lines;
  lines.reserve(4);
  auto addLine = [&](const std::string& raw, int fontId, EpdFontFamily::Style style) {
    if (raw.empty()) return;
    std::string t = renderer.truncatedText(fontId, raw.c_str(), maxTextW, style);
    lines.push_back(
        {t, fontId, style, renderer.getTextWidth(fontId, t.c_str(), style), renderer.getLineHeight(fontId)});
  };
  addLine(infoTitle, titleFont, EpdFontFamily::BOLD);
  addLine(infoAuthor, bodyFont, EpdFontFamily::REGULAR);

  // Metric line(s) per the Content setting, worded for the chosen chapter/book reference.
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
    addLine(std::to_string(infoPagesLeft) + " " + I18N.get(suffix), bodyFont, EpdFontFamily::REGULAR);
  }
  if (wantTime && infoMinutesLeft >= 0) {
    const StrId suffix = refBook ? StrId::STR_MIN_LEFT_IN_BOOK : StrId::STR_MIN_LEFT_IN_CHAPTER;
    addLine("~" + std::to_string(infoMinutesLeft) + " " + I18N.get(suffix), bodyFont, EpdFontFamily::REGULAR);
  }

  int textW = 0;
  int textH = 0;
  for (size_t i = 0; i < lines.size(); i++) {
    textW = std::max(textW, lines[i].width);
    textH += lines[i].height + (i > 0 ? lineGap : 0);
  }

  const int paneW = std::min(maxPaneW, textW + 2 * padX);
  const int paneH = 2 * padY + textH;
  const int paneX = (screenW - paneW) / 2;
  const int paneY = (SETTINGS.sleepInfoPanePosition == CrossPointSettings::SLEEP_INFO_PANE_TOP)
                        ? edgeMargin
                        : screenH - edgeMargin - paneH;

  // Opaque white card with a thin rounded border — clean and legible over any photo.
  renderer.fillRoundedRect(paneX, paneY, paneW, paneH, radius, Color::White);
  renderer.drawRoundedRect(paneX, paneY, paneW, paneH, 1, radius, true);

  // Stack the lines, each centered within the pane.
  int lineTop = paneY + padY;
  for (size_t i = 0; i < lines.size(); i++) {
    if (i > 0) lineTop += lines[i - 1].height + lineGap;
    renderer.drawText(lines[i].fontId, paneX + (paneW - lines[i].width) / 2, lineTop, lines[i].text.c_str(), true,
                      lines[i].style);
  }
}
