#pragma once

#include "aabb.hpp"
#include "block.hpp"
#include "mesh.hpp"
#include "shader.hpp"
#include "texture.hpp"
#include <cstdint>
#include <vector>

class World;

using ChunkKey = uint64_t;

inline ChunkKey makeChunkKey(int x, int z) {
  return (uint64_t(uint32_t(x)) << 32) | uint32_t(z);
}

inline int getChunkX(ChunkKey key) { return int(key >> 32); }

inline int getChunkZ(ChunkKey key) { return int(key & 0xFFFFFFFF); }

inline bool letsNeighborRender(BlockType t) {
  // Which blocks should cause neighbors to show their faces?
  // Air always does. Glass/leaves would too if you add them.
  return (t == BlockType::AIR);
}

/**
 * chunk.hpp
 *
 * A chunk for the 3d world
 *
 */
class Chunk {
public:
  Chunk(const int width, const int length, const int height, const int world_x,
        const int world_z, World *world);
  ~Chunk();

  void generateMesh(const Texture &atlas);
  void drawOpaque(Shader &shader);
  void drawTransparent(Shader &shader);
  BlockType getBlock(int x, int y, int z) const;
  void setBlock(int x, int y, int z, BlockType type);
  ChunkKey getChunkKey() const;

  Mesh opaqueMesh;
  Mesh transparentMesh;
  AABB m_aabb; // Add AABB member

private:
  bool faceVisible(int x, int y, int z, int dir,
                   BlockType currentBlockType) const;
  // blocks[x][z][y]
  std::vector<uint8_t> blocks;
  int blockIndex(int x, int y, int z) const {
    return y * width * length + z * width + x;
  }
  const int width;
  const int length;
  int height;
  const int world_x;
  const int world_z;
  World *world;
};
