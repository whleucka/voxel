#pragma once

// Helpers shared by test files.

#include "biome/biome_manager.hpp"
#include "core/constants.hpp"
#include "core/settings.hpp"
#include "world/world.hpp"
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <optional>
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

// Fills the box [lo, hi] (inclusive, world blocks) with one block type by
// writing straight into the loaded chunks and relighting each chunk once.
// Much faster than World::setBlockAt (which relights per block) for building
// test rigs; edits aren't recorded for saving.
inline void fill(World &world, glm::ivec3 lo, glm::ivec3 hi, BlockType type) {
  auto floorDiv = [](int a, int b) { return a >= 0 ? a / b : -((-a + b - 1) / b); };
  for (int cx = floorDiv(lo.x, kChunkWidth); cx <= floorDiv(hi.x, kChunkWidth); ++cx)
    for (int cz = floorDiv(lo.z, kChunkDepth); cz <= floorDiv(hi.z, kChunkDepth); ++cz) {
      auto chunk = world.getChunk(cx, cz);
      if (!chunk) continue;
      std::lock_guard lock(chunk->data_mutex);
      for (int x = std::max(lo.x, cx * kChunkWidth); x <= std::min(hi.x, cx * kChunkWidth + kChunkWidth - 1); ++x)
        for (int z = std::max(lo.z, cz * kChunkDepth); z <= std::min(hi.z, cz * kChunkDepth + kChunkDepth - 1); ++z)
          for (int y = std::max(lo.y, 0); y <= std::min(hi.y, kChunkHeight - 1); ++y)
            chunk->at(x - cx * kChunkWidth, y, z - cz * kChunkDepth) = type;
      chunk->computeSkyLight();
      chunk->computeBlockLight();
    }
}

// The first chunk (spiralling out from the origin) whose biome is `b`.
inline std::optional<glm::ivec2> findBiomeChunk(BiomeType b) {
  for (int r = 0; r < 300; ++r)
    for (int dx = -r; dx <= r; ++dx)
      for (int dz = -r; dz <= r; ++dz) {
        if (std::abs(dx) != r && std::abs(dz) != r) continue;
        if (BiomeManager::getBiomeForChunk(dx, dz) == b) return glm::ivec2(dx, dz);
      }
  return std::nullopt;
}

// A ground cell in chunk c, high in the sky, set to the given block.
inline glm::ivec3 groundIn(World &world, glm::ivec2 c, BlockType b) {
  world.loadChunkSync(c.x, c.y);
  glm::ivec3 g{c.x * kChunkWidth + 5, 180, c.y * kChunkDepth + 5};
  fill(world, g, g, b);
  return g;
}

