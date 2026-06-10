#pragma once

#include <cstdint>
#include <functional>
#include <string>

// Oversized block numerals for 1-bit e-ink, drawn as classic seven-segment bars via a fillRect
// callback. The built-in fonts top out at 18pt, far too small for a dashboard centerpiece; solid
// rectangles render crisp at any size with no font data. Pure logic — host-testable.
namespace BigDigits {

// Segment bit layout (standard seven-segment):
//   _a_
//  f| |b
//   -g-
//  e| |c
//   _d_
enum Segment : uint8_t { SEG_A = 1, SEG_B = 2, SEG_C = 4, SEG_D = 8, SEG_E = 16, SEG_F = 32, SEG_G = 64 };

inline uint8_t segmentsFor(const char c) {
  switch (c) {
    case '0': return SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F;
    case '1': return SEG_B | SEG_C;
    case '2': return SEG_A | SEG_B | SEG_G | SEG_E | SEG_D;
    case '3': return SEG_A | SEG_B | SEG_G | SEG_C | SEG_D;
    case '4': return SEG_F | SEG_G | SEG_B | SEG_C;
    case '5': return SEG_A | SEG_F | SEG_G | SEG_C | SEG_D;
    case '6': return SEG_A | SEG_F | SEG_G | SEG_E | SEG_C | SEG_D;
    case '7': return SEG_A | SEG_B | SEG_C;
    case '8': return SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F | SEG_G;
    case '9': return SEG_A | SEG_B | SEG_C | SEG_D | SEG_F | SEG_G;
    default: return 0;
  }
}

using FillRectFn = std::function<void(int x, int y, int w, int h)>;

// Digit width for a given height (classic 1:2 proportions). Bar thickness scales with height.
inline int thicknessFor(const int height) { return height / 7 < 2 ? 2 : height / 7; }
inline int digitWidth(const int height) { return height / 2 + thicknessFor(height); }

// Draw one digit with its top-left corner at (x, y).
inline void drawDigit(const char c, const int x, const int y, const int height, const FillRectFn& fillRect) {
  const int t = thicknessFor(height);
  const int w = digitWidth(height);
  const int half = (height - t) / 2;
  const uint8_t seg = segmentsFor(c);
  if (seg & SEG_A) fillRect(x, y, w, t);
  if (seg & SEG_B) fillRect(x + w - t, y, t, half + t / 2);
  if (seg & SEG_C) fillRect(x + w - t, y + half + t / 2, t, height - half - t / 2);
  if (seg & SEG_D) fillRect(x, y + height - t, w, t);
  if (seg & SEG_E) fillRect(x, y + half + t / 2, t, height - half - t / 2);
  if (seg & SEG_F) fillRect(x, y, t, half + t / 2);
  if (seg & SEG_G) fillRect(x, y + half, w, t);
}

// '%' drawn as two small squares and a diagonal of stacked rects.
inline void drawPercent(const int x, const int y, const int height, const FillRectFn& fillRect) {
  const int t = thicknessFor(height);
  const int w = digitWidth(height);
  const int box = t * 2;
  fillRect(x, y, box, box);
  fillRect(x + w - box, y + height - box, box, box);
  const int steps = 6;
  for (int i = 0; i < steps; i++) {
    const int sx = x + (w - t) * (steps - 1 - i) / (steps - 1);
    const int sy = y + (height - t) * i / (steps - 1);
    int stepH = height / steps + 1;
    if (sy + stepH > y + height) stepH = y + height - sy;  // clamp to the digit box
    fillRect(sx, sy, t, stepH);
  }
}

// Total width of a numeral string (digits and '%' only) at the given height.
inline int textWidth(const std::string& text, const int height, const int gap) {
  if (text.empty()) return 0;
  const int w = digitWidth(height);
  return static_cast<int>(text.size()) * w + (static_cast<int>(text.size()) - 1) * gap;
}

// Draw a numeral string (digits and '%') with its top-left at (x, y). Returns drawn width.
inline int drawText(const std::string& text, const int x, const int y, const int height, const int gap,
                    const FillRectFn& fillRect) {
  int cx = x;
  const int w = digitWidth(height);
  for (const char c : text) {
    if (c == '%') {
      drawPercent(cx, y, height, fillRect);
    } else {
      drawDigit(c, cx, y, height, fillRect);
    }
    cx += w + gap;
  }
  return cx - gap - x;
}

}  // namespace BigDigits
