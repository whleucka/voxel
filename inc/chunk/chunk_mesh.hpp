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
  void renderOpaque();
  void renderTransparent();

  bool isUploaded() const { return gpuUploaded; }
  bool hasTransparent() const { return !transparentIndices.empty(); }

private:
  void setupVAO(GLuint vao, GLuint vbo, GLuint ebo,
                const std::vector<BlockVertex>& verts,
                const std::vector<unsigned int>& inds);

  // GL objects - opaque
  GLuint VAO{}, VBO{}, EBO{};
  // GL objects - transparent
  GLuint transparentVAO{}, transparentVBO{}, transparentEBO{};

  // CPU data - opaque
  std::vector<BlockVertex> vertices;
  std::vector<unsigned int> indices;
  // CPU data - transparent
  std::vector<BlockVertex> transparentVertices;
  std::vector<unsigned int> transparentIndices;

  // simple state
  std::atomic<bool> cpuReady{false};
  std::atomic<bool> gpuUploaded{false};
};
