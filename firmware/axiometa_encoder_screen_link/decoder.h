#pragma once
#include <stdint.h>
// Design: x4 Gray-code decoding; bounce reversals cancel, two-bit jumps are rejected.
inline int8_t quadratureStep(uint8_t previous, uint8_t next) {
  static const int8_t table[16] = {0,-1,1,0, 1,0,0,-1, -1,0,0,1, 0,1,-1,0};
  return table[(previous << 2) | next];
}
struct DebouncedButton {
  bool stable = false, candidate = false;
  uint32_t since = 0;
  void reset(bool level, uint32_t now) { stable = candidate = level; since = now; }
  bool update(bool level, uint32_t now, uint32_t debounceMs) {
    if (level != candidate) { candidate = level; since = now; }
    if (candidate != stable && uint32_t(now - since) >= debounceMs) {
      stable = candidate; return true;
    }
    return false;
  }
};
