#pragma once

#include <vector>
#include "block.hpp"
#include "shader.hpp"
#include "texture.hpp"
#include "mesh.hpp"

constexpr int CHUNK_WIDTH = 16;
constexpr int CHUNK_HEIGHT = 256;
constexpr int CHUNK_LENGTH = 16;

class Chunk {
public:
  Chunk(const Texture& atlas);
  ~Chunk();

  void draw(Shader &shader);

private:
  std::vector<Block> m_blocks;
  Mesh m_mesh;
};