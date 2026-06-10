#pragma once

// Pure typography layout decisions, kept free of parser/renderer dependencies so they can be
// unit-tested on the host (see test/typography/).
namespace Typography {

// Orphan control: returns true when placing this line here would strand a paragraph's first
// line as the last line of the page (an "orphan"). The caller then breaks the page first so
// the paragraph starts on the next page intact.
//
// lineIndexInParagraph: 0-based index of this line within its paragraph.
// paragraphHasMoreLines: at least one more line of this paragraph follows.
// currentY: y position the line would be placed at.
// lineHeight / viewportHeight: in the same units as currentY.
//
// Never fires on an empty page (currentY == 0): the paragraph has to start somewhere, which
// also guarantees a deferred paragraph can't be deferred again (no infinite loop).
inline bool shouldBreakForOrphan(const int lineIndexInParagraph, const bool paragraphHasMoreLines,
                                 const int currentY, const int lineHeight, const int viewportHeight) {
  if (lineIndexInParagraph != 0 || !paragraphHasMoreLines) return false;
  if (currentY <= 0) return false;
  if (lineHeight <= 0 || viewportHeight <= 0) return false;
  if (currentY + lineHeight > viewportHeight) return false;  // doesn't fit anyway; the normal break handles it
  // Fits, but nothing after it would: this slot is the last on the page.
  return currentY + 2 * lineHeight > viewportHeight;
}

}  // namespace Typography
