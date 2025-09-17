#include "chunk.hpp"
#include "greedy_mesher.hpp"

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
  for (int x = 0; x < W; ++x) {
    for (int z = 0; z < L; ++z) {
      for (int y = 0; y < H; ++y) {
        setBlock(x, y, z, BlockType::STONE);
      }
    }
  }
}

void Chunk::generateMesh() {
  GreedyMesher::build(*this, opaqueMesh, transparentMesh);
}

void Chunk::drawOpaque() const {
  // This will be handled by the renderer
}

void Chunk::drawTransparent() const {
  // This will be handled by the renderer
}
