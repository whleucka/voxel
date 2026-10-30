#pragma once

#include "glad/glad.h"
#include <array>
#include <optional>

// Times spans of GPU work with timestamp queries. A frame's results are read
// kFrames frames later, when they are in, so the CPU never waits on the GPU;
// a result that isn't in by then is dropped. Main thread, with a GL context.
class GpuTimer {
public:
  static constexpr int kSpans = 4;
  static constexpr int kFrames = 6;

  void init();
  void shutdown();

  void begin(int span) { stamp(span, 0); }
  void end(int span) { stamp(span, 1); }

  // Moves on to the next frame. Returns how long each span took, in ms, in
  // the frame whose queries are about to be reused (if it ran and is in).
  std::array<std::optional<double>, kSpans> endFrame();

private:
  void stamp(int span, int edge);

  GLuint queries[kFrames][kSpans][2] = {};
  bool used[kFrames][kSpans] = {};
  int frame = 0;
  bool ready = false;
};
