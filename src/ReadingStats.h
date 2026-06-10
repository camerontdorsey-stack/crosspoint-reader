#pragma once

#include <HalStorage.h>

#include <cstdint>
#include <cstring>
#include <string>

// Reading statistics persisted at /.crosspoint/readstats.bin. The X4 has no RTC, so there is no
// calendar — stats are "this session" (since boot, reset when the reader opens after a cold start)
// and "all time" (monotonic accumulators). The reader updates the struct in RAM and persists it
// piggybacked on progress saves; the sleep screens read it back for display.
namespace ReadingStats {

constexpr char STATS_FILE[] = "/.crosspoint/readstats.bin";
constexpr uint8_t STATS_VERSION = 1;

struct Stats {
  uint32_t lifetimePages = 0;
  uint64_t lifetimeMs = 0;
  uint32_t sessionPages = 0;
  uint32_t sessionMs = 0;
};

// 1 version byte + the four counters, packed little-endian.
constexpr size_t STATS_BYTES = 1 + 4 + 8 + 4 + 4;

inline bool load(Stats& out) {
  HalFile f;
  if (!Storage.openFileForRead("RST", STATS_FILE, f)) return false;
  uint8_t buf[STATS_BYTES];
  const bool ok = f.read(buf, STATS_BYTES) == STATS_BYTES && buf[0] == STATS_VERSION;
  f.close();
  if (!ok) return false;
  memcpy(&out.lifetimePages, buf + 1, 4);
  memcpy(&out.lifetimeMs, buf + 5, 8);
  memcpy(&out.sessionPages, buf + 13, 4);
  memcpy(&out.sessionMs, buf + 17, 4);
  return true;
}

inline bool save(const Stats& s) {
  HalFile f;
  if (!Storage.openFileForWrite("RST", STATS_FILE, f)) return false;
  uint8_t buf[STATS_BYTES];
  buf[0] = STATS_VERSION;
  memcpy(buf + 1, &s.lifetimePages, 4);
  memcpy(buf + 5, &s.lifetimeMs, 8);
  memcpy(buf + 13, &s.sessionPages, 4);
  memcpy(buf + 17, &s.sessionMs, 4);
  const bool ok = f.write(buf, STATS_BYTES) == STATS_BYTES;
  f.close();
  return ok;
}

// "38m" under an hour, "5h 20m" above. minutes < 0 -> empty string.
inline std::string formatDuration(const long minutes) {
  if (minutes < 0) return "";
  if (minutes < 60) return std::to_string(minutes) + "m";
  const long h = minutes / 60;
  const long m = minutes % 60;
  if (m == 0) return std::to_string(h) + "h";
  return std::to_string(h) + "h " + std::to_string(m) + "m";
}

// Per-boot accumulator. First access loads lifetime counters from disk and zeroes the session
// counters (a session = one power-on). Function-local static, so this resets on every boot.
inline Stats& current() {
  static Stats s = [] {
    Stats loaded;
    load(loaded);  // best effort; zeros on first run
    loaded.sessionPages = 0;
    loaded.sessionMs = 0;
    return loaded;
  }();
  return s;
}

}  // namespace ReadingStats
