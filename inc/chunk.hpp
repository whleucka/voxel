#pragma once

#include "block.hpp"
#include "mesh.hpp"
#include "shader.hpp"
#include "texture.hpp"
#include <vector>

/**
 * chunk.hpp
 *
 * A chunk is a vector of blocks
 * A minecraft chunk is 16 x 16 x 256 or 384
 *
 */
class Chunk {
public:
  Chunk(const int width, const int length, const int height,
        const Texture &atlas);
  ~Chunk();

  void draw(Shader &shader);

private:
  std::vector<Block> blocks;
  Mesh mesh;
  const int width;
  const int length;
  int height;
};
