#pragma once

#include "block/block_vertex.hpp"
#include "render/texture_manager.hpp"
#include <atomic>
#include "block/block_type.hpp"
#include <glad/glad.h>
#include <vector>

class Chunk;
class World;

enum Face { Top, Bottom, Left, Right, Front, Back };

class ChunkMesh {
public:
  ChunkMesh();
  ~ChunkMesh();

  static BlockType getBlock(World*, const Chunk&, int, int, int);
  void generateCPU(World* world, const Chunk& chunk, TextureManager& texture_manager); // CPU-only
  void upload();     // GL only (main thread)
  void render();     // draw only when uploaded

  bool isUploaded() const { return gpuUploaded; }

private:
  // GL objects
  GLuint VAO{}, VBO{}, EBO{};

  // CPU data
  std::vector<BlockVertex> vertices;
  std::vector<unsigned int> indices;

  // simple state
  std::atomic<bool> cpuReady{false};
  std::atomic<bool> gpuUploaded{false};
};
