#pragma once

#include "block/block_type.hpp"
#include "chunk/chunk_mesh.hpp"
#include <glm/ext/matrix_transform.hpp>
#include <glm/glm.hpp>
#include <vector>

struct ChunkKey {
  int x, z;
  bool operator==(const ChunkKey &other) const noexcept {
    return x == other.x && z == other.z;
  }
};

struct ChunkKeyHash {
  std::size_t operator()(const ChunkKey &k) const noexcept {
    return (std::hash<int>()(k.x) ^ (std::hash<int>()(k.z) << 1));
  }
};

class Chunk {
public:
  Chunk(int x, int z);
  ~Chunk();

  void generate();
  void uploadGPU(TextureManager &texture_manager);

  BlockType &at(int x, int y, int z);
  const BlockType &at(int x, int y, int z) const;

  ChunkMesh &getMesh() { return mesh; }
  glm::mat4 getModelMatrix() const;
  glm::vec2 getPos() { return pos; }

private:
  glm::vec2 pos;
  std::vector<BlockType> blocks;
  ChunkMesh mesh;
};
