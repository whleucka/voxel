#pragma once

// Helpers shared by test files.

#include "core/constants.hpp"
#include "core/settings.hpp"
#include "world/world.hpp"
#include <chrono>
#include <filesystem>
#include <random>
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

// A fresh directory under the system temp dir, removed on scope exit.
struct TempDir {
  std::filesystem::path path;
  TempDir() {
    std::random_device rd;
    path = std::filesystem::temp_directory_path() /
           ("voxel_test_" + std::to_string(rd()));
    std::filesystem::create_directories(path);
  }
  ~TempDir() { std::filesystem::remove_all(path); }
};

// Restores the global world seed / noise offset that Game::load overwrites.
struct ScopedSeed {
  uint32_t seed = g_settings.world_seed;
  glm::vec2 offset = g_settings.noise_offset;
  ~ScopedSeed() {
    g_settings.world_seed = seed;
    g_settings.noise_offset = offset;
  }
};
