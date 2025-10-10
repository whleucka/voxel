#include "biome/tree_spawner.hpp"
#include "block/block_type.hpp"
#include "core/constants.hpp"
#include <glm/gtc/noise.hpp>

TreeSpawner::TreeSpawner(double den, std::unique_ptr<TreeGenerator> gen)
    : density(den), generator(std::move(gen)) {}

void TreeSpawner::spawn(
    Chunk &chunk, std::function<bool(Chunk &, int, int, int)> isValidSpawn) {
  for (int x = kBiomeTreeBorder; x <= kChunkWidth - 1 - kBiomeTreeBorder; ++x) {
    for (int z = kBiomeTreeBorder; z <= kChunkDepth - 1 - kBiomeTreeBorder;
         ++z) {
      int y = 0;
      glm::vec2 pos = chunk.getPos();
      const int world_x = pos[0] * kChunkWidth + x;
      const int world_z = pos[1] * kChunkDepth + z;

      for (int i = kChunkHeight - 1; i > 0; --i) {
        if (chunk.at(x, i, z) != BlockType::AIR) {
          y = i;
          break;
        }
      }

      if (y > 0 && isValidSpawn(chunk, x, y, z)) {
        const double tree_noise =
            (glm::perlin(glm::vec3((world_x * kChunkWidth + x) * 0.8, y * 0.02,
                                   (world_z * kChunkDepth + z) * 0.8)) *
                 0.5 +
             0.5);

        if (tree_noise > density) {
          generator->generate(chunk, x, y, z);
        }
      }
    }
  }
}
