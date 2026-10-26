#pragma once

// Helpers shared by test files.

#include "core/constants.hpp"
#include "core/settings.hpp"
#include "world/world.hpp"
#include <chrono>
#include <thread>

// Chunk generation is async; wait (bounded) for a chunk to appear.
inline bool waitForChunk(const World &world, int cx, int cz) {
  using namespace std::chrono;
  auto deadline = steady_clock::now() + seconds(20);
  while (steady_clock::now() < deadline) {
    if (world.getChunk(cx, cz)) return true;
    std::this_thread::sleep_for(milliseconds(5));
  }
  return false;
}

// Shrinks the streaming radius for one test; World reads it at construction.
struct ScopedRenderDistance {
  int saved = g_settings.render_distance;
  explicit ScopedRenderDistance(int chunks) {
    g_settings.render_distance = chunks * kChunkWidth;
  }
  ~ScopedRenderDistance() { g_settings.render_distance = saved; }
};
