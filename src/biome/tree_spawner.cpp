#include "biome/tree_spawner.hpp"
#include "block/block_type.hpp"
#include "core/constants.hpp"
#include "core/settings.hpp"
#include <glm/gtc/noise.hpp>

TreeSpawner::TreeSpawner(double den, std::unique_ptr<TreeGenerator> gen)
    : density(den), generator(std::move(gen)) {}

void TreeSpawner::spawn(
    Chunk &chunk, std::function<bool(Chunk &, int, int, int)> isValidSpawn) {
  glm::vec2 pos = chunk.getPos();

  for (int x = kBiomeTreeBorder; x <= kChunkWidth - 1 - kBiomeTreeBorder; ++x) {
    for (int z = kBiomeTreeBorder; z <= kChunkDepth - 1 - kBiomeTreeBorder;
         ++z) {
      // Find the topmost non-air block (skip water too)
      int y = 0;
      for (int i = kChunkHeight - 1; i > 0; --i) {
        BlockType b = chunk.at(x, i, z);
        if (b != BlockType::AIR && b != BlockType::WATER) {
          y = i;
          break;
        }
      }

      if (y <= 0) continue;
      if (!isValidSpawn(chunk, x, y, z)) continue;

      // World-space coordinates for deterministic noise sampling
      const float world_x = pos[0] * kChunkWidth + x + g_settings.noise_offset.x;
      const float world_z = pos[1] * kChunkDepth + z + g_settings.noise_offset.y;

      // 2D noise — tree placement should not depend on height
      // Use a moderate frequency so trees are spaced naturally
      const double tree_noise =
          glm::perlin(glm::vec2(world_x * 0.15f, world_z * 0.15f)) *
              0.5 +
          0.5;

      if (tree_noise > density) {
        generator->generate(chunk, x, y, z);
      }
    }
  }
}
