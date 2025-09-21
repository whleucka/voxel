#pragma once

static inline int floorDiv(int a, int b) {
  int q = a / b, r = a % b;
  if (r != 0 && ((r < 0) != (b < 0)))
    --q;
  return q;
}

static inline int floorMod(int a, int b) {
  int r = a % b;
  return (r < 0) ? r + b : r;
}
