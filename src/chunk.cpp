#include "chunk.hpp"
#include "block_type.hpp"
#include <glm/ext/vector_float3.hpp>
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
