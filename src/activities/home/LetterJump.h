#pragma once

#include <cctype>
#include <cstddef>
#include <string>
#include <vector>

// Pure logic for the file-browser "first-letter jog". Kept header-only and free of any
// firmware/UI dependency so it can be unit-tested on the host (see test/letter_jump/).
namespace LetterJump {

// Case-folded first character of an entry; empty entries sort as NUL.
inline unsigned char firstKey(const std::string& s) {
  return static_cast<unsigned char>(std::tolower(static_cast<unsigned char>(s.empty() ? '\0' : s[0])));
}

// Given an alphabetically-sorted list and the current selection, return the index to jump to.
// Forward: first entry of the next distinct starting letter (or the last entry if already in the
// final group). Backward: snap to the current letter's first entry, or — if already there — the
// first entry of the previous letter group. Returns 0 for an empty list.
inline size_t jump(const std::vector<std::string>& files, size_t from, bool forward) {
  if (files.empty()) return 0;
  const unsigned char current = firstKey(files[from]);

  if (forward) {
    for (size_t i = from + 1; i < files.size(); i++) {
      if (firstKey(files[i]) != current) return i;
    }
    return files.size() - 1;
  }

  size_t groupStart = from;
  while (groupStart > 0 && firstKey(files[groupStart - 1]) == current) groupStart--;
  if (from != groupStart) return groupStart;
  if (groupStart == 0) return 0;
  const unsigned char prevKey = firstKey(files[groupStart - 1]);
  size_t i = groupStart - 1;
  while (i > 0 && firstKey(files[i - 1]) == prevKey) i--;
  return i;
}

}  // namespace LetterJump
