inline int floorDiv(int a, int b) {
  int q = a / b;
  int r = a % b;
  if ((r != 0) && ((r < 0) != (b < 0))) q -= 1;
  return q;
}

inline int floorMod(int a, int b) {
  int r = a % b;
  return (r < 0) ? r + b : r;
}

// World <-> Chunk conversions
inline int worldToChunk(int p, int size) { return floorDiv(p, size); }
inline int worldToLocal(int p, int size) { return floorMod(p, size); }
