#include "chunk.hpp"
#include "greedy_mesher.hpp"
#include <algorithm>
#include <glm/gtc/noise.hpp>

Chunk::Chunk(int32_t world_x, int32_t world_z)
    : world_x(world_x), world_z(world_z) {
  blocks.fill(BlockType::AIR);
}

BlockType Chunk::getBlock(int x, int y, int z) const {
  if (x < 0 || x >= W || y < 0 || y >= H || z < 0 || z >= L) {
    return BlockType::AIR;
  }
  return blocks[getIndex(x, y, z)];
}

void Chunk::setBlock(int x, int y, int z, BlockType type) {
  if (x < 0 || x >= W || y < 0 || y >= H || z < 0 || z >= L) {
    return;
  }
  blocks[getIndex(x, y, z)] = type;
}


void Chunk::generateChunk() {
  // World-space coords for this chunk
  const int baseX = world_x * W;
  const int baseZ = world_z * L;

  const int SEA_LEVEL = 64;     // tweak
  const float FREQ    = 0.05f;  // noise frequency
  const float AMP     = 12.0f;  // height amplitude

  for (int x = 0; x < W; ++x) {
    for (int z = 0; z < L; ++z) {
      // perlin returns [-1,1]; scale & shift
      float n = glm::perlin(glm::vec2((baseX + x) * FREQ, (baseZ + z) * FREQ));
      int h = SEA_LEVEL + int(n * AMP);
      h = std::clamp(h, 1, H - 2);

      // stone core
      for (int y = 0; y < h - 4; ++y) setBlock(x, y, z, BlockType::STONE);
      // dirt layer
      for (int y = std::max(0, h - 4); y < h; ++y) setBlock(x, y, z, BlockType::DIRT);
      // grass top
      setBlock(x, h, z, BlockType::GRASS);

      // water fill up to sea level if needed
      if (h < SEA_LEVEL) {
        for (int y = h + 1; y <= SEA_LEVEL; ++y) setBlock(x, y, z, BlockType::WATER);
      }
    }
  }
}

void Chunk::generateMesh() {
  auto sample = [&](int gx, int gy, int gz) -> BlockType {
    return getBlock(gx, gy, gz);   // world-space accessor (can cross chunks)
  };
  GreedyMesher::build(*this, sample, opaqueMesh, transparentMesh);
}

void Chunk::drawOpaque() const {
  // This will be handled by the renderer
}

void Chunk::drawTransparent() const {
  // This will be handled by the renderer
}
