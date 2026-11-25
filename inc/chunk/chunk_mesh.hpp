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

  // Builds the vertices from a copy of the chunk and the edges of its
  // neighbours, each copied under its own lock.
  void generateCPU(World* world, const Chunk& chunk, TextureManager& texture_manager); // CPU-only
  void upload();     // GL only (main thread)
  void renderOpaque();
  void renderTransparent();

  bool isUploaded() const { return gpuUploaded; }
  bool hasTransparent() const { return transparent_index_count > 0; }
  // The heights (in blocks) the uploaded faces span, for culling: a chunk's
  // terrain fills a band, not the whole 0..kChunkHeight column. Main thread,
  // like the counts; empty (lo > hi) until something is uploaded.
  float minY() const { return gpu_min_y; }
  float maxY() const { return gpu_max_y; }
  // The blended faces (water) as built by generateCPU, for tests.
  const std::vector<BlockVertex> &transparentVertexData() const { return transparentVertices; }
  // The opaque faces, likewise.
  const std::vector<BlockVertex> &vertexData() const { return vertices; }

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

  // Index counts snapshotted at upload() time — only ever written on the main
  // thread, so the render methods never touch the live CPU vectors.
  GLsizei opaque_index_count{0};
  GLsizei transparent_index_count{0};
  float gpu_min_y{1.0f}, gpu_max_y{0.0f};
  // The same as built by generateCPU, snapshotted by upload().
  float cpu_min_y{1.0f}, cpu_max_y{0.0f};

  // simple state
  std::atomic<bool> cpuReady{false};
  std::atomic<bool> gpuUploaded{false};
};
