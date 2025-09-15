#pragma once

#include "aabb.hpp" // Include AABB header
#include "block.hpp"
#include "mesh.hpp"
#include "shader.hpp"
#include "texture.hpp"
#include <vector>

class World;
struct ChunkKey;

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
  bool faceVisible(int x, int y, int z, int dir, BlockType currentBlockType) const;
  // blocks[x][z][y]
  std::vector<std::vector<std::vector<BlockType>>> blocks;
  const int width;
  const int length;
  int height;
  const int world_x;
  const int world_z;
  World *world;
};
