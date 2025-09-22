#pragma once

#include <array>
#include <cstdint>

#include <glm/vec2.hpp>

#include "block_type.hpp"
#include "mesh.hpp"

class Chunk {
public:
  static constexpr int W = 16;
  static constexpr int H = 256;
  static constexpr int L = 16;

  Chunk(int32_t world_x, int32_t world_z);

  BlockType getBlock(int x, int y, int z) const;
  void setBlock(int x, int y, int z, BlockType type);

  void generateChunk();
  void generateTree(int x, int y, int z);

  int32_t world_x, world_z;
  Mesh opaqueMesh, transparentMesh;

private:
  std::array<BlockType, W * H * L> blocks;
  void generateTerrain(int x, int z);
  BlockType generateTopBlock(int x, int y, int z);
  BlockType generateInternalBlock(int x, int y, int z);

  inline int getIndex(int x, int y, int z) const { return x + W * (z + L * y); }
};

struct ChunkKey {
  int32_t x, z;
  bool operator==(const ChunkKey &other) const {
    return x == other.x && z == other.z;
  }
};
