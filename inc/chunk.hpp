#pragma once

#include "block.hpp"
#include "mesh.hpp"
#include "shader.hpp"
#include "texture.hpp"
#include <vector>

class World;

class Chunk {
public:
  Chunk(const int width, const int length, const int height,
        const int world_x, const int world_z, World *world);
  ~Chunk();

  void generate_mesh(const Texture &atlas);
  void draw(Shader &shader);
  BlockType get_block(int x, int y, int z) const;

private:
  bool is_face_visible(int x, int y, int z, int face);

  std::vector<std::vector<std::vector<BlockType>>> blocks;
  Mesh mesh;
  const int width;
  const int length;
  int height;
  const int world_x;
  const int world_z;
  World *world;
};
