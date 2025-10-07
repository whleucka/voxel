#pragma once

#include "block/block_vertex.hpp"
#include "render/texture_manager.hpp"
#include <glad/glad.h>
#include <vector>

class Chunk;

enum Face { Top, Bottom, Left, Right, Front, Back };

class ChunkMesh {
public:
  ChunkMesh();
  ~ChunkMesh();

  void generate(Chunk &chunk, TextureManager &texture_manager);
  void upload();
  void render();

private:
  unsigned int VAO, VBO, EBO = 0;
  std::vector<BlockVertex> vertices;
  std::vector<unsigned int> indices;
};
