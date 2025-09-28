#include "tree_spawner.hpp"
#include "tree_generator.hpp"
#include "world_constants.hpp"
#include "block_type.hpp"
#include <glm/gtc/noise.hpp>

TreeSpawner::TreeSpawner(double density) : m_density(density) {}

void TreeSpawner::spawn(Chunk &chunk) {
  TreeGenerator tree_generator;
  for (int x = TREE_BORDER_THRESHOLD; x <= Chunk::W - 1 - TREE_BORDER_THRESHOLD;
       ++x) {
    for (int z = TREE_BORDER_THRESHOLD;
         z <= Chunk::L - 1 - TREE_BORDER_THRESHOLD; ++z) {
      int y = 0;
      for (int i = Chunk::H - 1; i > 0; --i) {
        if (chunk.getBlock(x, i, z) != BlockType::AIR) {
          y = i;
          break;
        }
      }

      if (y > 0 && chunk.getBlock(x, y, z) == BlockType::GRASS) {
        const double tree_noise =
            (glm::perlin(glm::vec3((chunk.world_x * Chunk::W + x) * 0.8,
                                   y * 0.02,
                                   (chunk.world_z * Chunk::L + z) * 0.8)) *
                 0.5 +
             0.5);

        if (tree_noise > m_density) {
          tree_generator.generate(chunk, x, y, z);
        }
      }
    }
  }
}
