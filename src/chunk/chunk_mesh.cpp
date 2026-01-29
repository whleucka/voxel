#include "chunk/chunk_mesh.hpp"
#include "block/block_data.hpp"
#include "chunk/chunk.hpp"
#include "core/constants.hpp"
#include "world/world.hpp"

namespace {

// Helper to pack a float position into int16 (multiply by 2 to preserve 0.5 precision)
inline int16_t packPos(float v) {
  return static_cast<int16_t>(v * 2.0f);
}

struct MeshBuffers {
  std::vector<BlockVertex>& vertices;
  std::vector<unsigned int>& indices;
  std::vector<BlockVertex>& transparentVertices;
  std::vector<unsigned int>& transparentIndices;
};

// Determine if a face should be rendered between current block and neighbor
inline bool shouldRenderFace(BlockType current, BlockType neighbor) {
  if (neighbor == BlockType::AIR) return true;
  if (isTransparent(current)) {
    // Transparent blocks: only render if neighbor is different type
    return neighbor != current;
  }
  // Opaque blocks: render if neighbor is transparent (visible through it)
  return isTransparent(neighbor);
}

void addQuad(MeshBuffers& buffers,
             int posX, int posY, int posZ,
             int sizeA, int sizeB,
             int tileX, int tileY,
             Face face,
             int16_t chunkWorldX, int16_t chunkWorldZ,
             bool transparent) {
  auto& vertices = transparent ? buffers.transparentVertices : buffers.vertices;
  auto& indices = transparent ? buffers.transparentIndices : buffers.indices;

  unsigned int start_index = static_cast<unsigned int>(vertices.size());
  uint8_t faceId = static_cast<uint8_t>(face);

  // Lambda to add a vertex with packed data
  auto V = [&](float px, float py, float pz, uint8_t uvX, uint8_t uvY) {
    vertices.push_back({
      packPos(px), packPos(py), packPos(pz),
      faceId,
      static_cast<uint8_t>(tileX), static_cast<uint8_t>(tileY),
      uvX, uvY,
      0, // padding
      chunkWorldX, chunkWorldZ
    });
  };

  switch (face) {
  case Face::Top:                                                           // +Y
    V(posX - 0.5f, posY + 0.5f, posZ - 0.5f,              0, 0);            // TL
    V(posX + sizeA - 0.5f, posY + 0.5f, posZ - 0.5f,      sizeA, 0);        // TR
    V(posX + sizeA - 0.5f, posY + 0.5f, posZ + sizeB - 0.5f, sizeA, sizeB); // BR
    V(posX - 0.5f, posY + 0.5f, posZ + sizeB - 0.5f,      0, sizeB);        // BL
    break;

  case Face::Bottom:                                                        // -Y
    V(posX - 0.5f, posY - 0.5f, posZ + sizeB - 0.5f,      0, 0);            // TL
    V(posX + sizeA - 0.5f, posY - 0.5f, posZ + sizeB - 0.5f, sizeA, 0);     // TR
    V(posX + sizeA - 0.5f, posY - 0.5f, posZ - 0.5f,      sizeA, sizeB);    // BR
    V(posX - 0.5f, posY - 0.5f, posZ - 0.5f,              0, sizeB);        // BL
    break;

  case Face::Front:                                                         // +Z
    V(posX - 0.5f, posY + sizeB - 0.5f, posZ + 0.5f,      0, 0);            // TL
    V(posX + sizeA - 0.5f, posY + sizeB - 0.5f, posZ + 0.5f, sizeA, 0);     // TR
    V(posX + sizeA - 0.5f, posY - 0.5f, posZ + 0.5f,      sizeA, sizeB);    // BR
    V(posX - 0.5f, posY - 0.5f, posZ + 0.5f,              0, sizeB);        // BL
    break;

  case Face::Back:                                                          // -Z
    V(posX + sizeA - 0.5f, posY + sizeB - 0.5f, posZ - 0.5f, 0, 0);         // TL
    V(posX - 0.5f, posY + sizeB - 0.5f, posZ - 0.5f,      sizeA, 0);        // TR
    V(posX - 0.5f, posY - 0.5f, posZ - 0.5f,              sizeA, sizeB);    // BR
    V(posX + sizeA - 0.5f, posY - 0.5f, posZ - 0.5f,      0, sizeB);        // BL
    break;

  case Face::Right:                                                         // +X
    V(posX + 0.5f, posY + sizeB - 0.5f, posZ - 0.5f,      0, 0);            // TL
    V(posX + 0.5f, posY + sizeB - 0.5f, posZ + sizeA - 0.5f, sizeA, 0);     // TR
    V(posX + 0.5f, posY - 0.5f, posZ + sizeA - 0.5f,      sizeA, sizeB);    // BR
    V(posX + 0.5f, posY - 0.5f, posZ - 0.5f,              0, sizeB);        // BL
    break;

  case Face::Left:                                                          // -X
    V(posX - 0.5f, posY + sizeB - 0.5f, posZ + sizeA - 0.5f, 0, 0);         // TL
    V(posX - 0.5f, posY + sizeB - 0.5f, posZ - 0.5f,      sizeA, 0);        // TR
    V(posX - 0.5f, posY - 0.5f, posZ - 0.5f,              sizeA, sizeB);    // BR
    V(posX - 0.5f, posY - 0.5f, posZ + sizeA - 0.5f,      0, sizeB);        // BL
    break;
  }

  // CCW winding indices
  switch (face) {
  case Face::Top:
  case Face::Bottom:
  case Face::Front:
  case Face::Back:
    indices.push_back(start_index + 0);
    indices.push_back(start_index + 3);
    indices.push_back(start_index + 2);
    indices.push_back(start_index + 2);
    indices.push_back(start_index + 1);
    indices.push_back(start_index + 0);
    break;

  case Face::Left:
  case Face::Right:
    indices.push_back(start_index + 0);
    indices.push_back(start_index + 1);
    indices.push_back(start_index + 2);
    indices.push_back(start_index + 2);
    indices.push_back(start_index + 3);
    indices.push_back(start_index + 0);
    break;
  }
}

} // namespace

ChunkMesh::ChunkMesh() {
  // GL objects created lazily in upload() on main thread
}

ChunkMesh::~ChunkMesh() {
  if (VAO) glDeleteVertexArrays(1, &VAO);
  if (VBO) glDeleteBuffers(1, &VBO);
  if (EBO) glDeleteBuffers(1, &EBO);
  if (transparentVAO) glDeleteVertexArrays(1, &transparentVAO);
  if (transparentVBO) glDeleteBuffers(1, &transparentVBO);
  if (transparentEBO) glDeleteBuffers(1, &transparentEBO);
}

BlockType ChunkMesh::getBlock(World *world, const Chunk &chunk, int x, int y, int z) {
  if (y < 0 || y >= kChunkHeight) {
    return BlockType::AIR;
  }

  const int chunk_x = chunk.getPos()[0];
  const int chunk_z = chunk.getPos()[1];

  const int world_x = chunk_x * kChunkWidth + x;
  const int world_z = chunk_z * kChunkDepth + z;

  const int target_chunk_x = floor((float)world_x / kChunkWidth);
  const int target_chunk_z = floor((float)world_z / kChunkDepth);

  if (target_chunk_x != chunk_x || target_chunk_z != chunk_z) {
    auto n = world->getChunk(target_chunk_x, target_chunk_z);
    const int local_x = world_x - target_chunk_x * kChunkWidth;
    const int local_z = world_z - target_chunk_z * kChunkDepth;
    return n ? n->at(local_x, y, local_z) : BlockType::AIR;
  }

  return chunk.at(x, y, z);
}

void ChunkMesh::generateCPU(World *world, const Chunk &chunk,
                            TextureManager &) {
  gpuUploaded = false;
  vertices.clear();
  indices.clear();
  transparentVertices.clear();
  transparentIndices.clear();

  // Compute chunk world offset for batch rendering
  const int16_t chunkWorldX = static_cast<int16_t>(chunk.getPos().x * kChunkWidth);
  const int16_t chunkWorldZ = static_cast<int16_t>(chunk.getPos().y * kChunkDepth);

  MeshBuffers buffers{vertices, indices, transparentVertices, transparentIndices};

  // Front face (+Z)
  for (int z = 0; z < kChunkDepth; ++z) {
    bool mask[kChunkWidth][kChunkHeight] = {false};
    for (int y = 0; y < kChunkHeight; ++y) {
      for (int x = 0; x < kChunkWidth; ++x) {
        if (mask[x][y]) continue;

        BlockType type = chunk.at(x, y, z);
        if (type == BlockType::AIR) continue;
        if (isLiquid(type)) continue; // Liquids only render top face

        BlockType neighbor = getBlock(world, chunk, x, y, z + 1);
        if (shouldRenderFace(type, neighbor)) {
          const auto &tex = block_data.at(type);

          int width = 1;
          while (x + width < kChunkWidth && !mask[x + width][y] &&
                 chunk.safeAt(x + width, y, z) == type &&
                 shouldRenderFace(type, getBlock(world, chunk, x + width, y, z + 1))) {
            width++;
          }

          int height = 1;
          bool can_expand = true;
          while (y + height < kChunkHeight && can_expand) {
            for (int i = 0; i < width; ++i) {
              if (mask[x + i][y + height] ||
                  chunk.safeAt(x + i, y + height, z) != type ||
                  !shouldRenderFace(type, getBlock(world, chunk, x + i, y + height, z + 1))) {
                can_expand = false;
                break;
              }
            }
            if (can_expand) height++;
          }

          for (int i = 0; i < height; ++i)
            for (int j = 0; j < width; ++j)
              mask[x + j][y + i] = true;

          addQuad(buffers, x, y, z, width, height,
                  tex.side.x, tex.side.y, Face::Front, chunkWorldX, chunkWorldZ,
                  isTransparent(type));
        }
      }
    }
  }

  // Back face (-Z)
  for (int z = kChunkDepth - 1; z >= 0; --z) {
    bool mask[kChunkWidth][kChunkHeight] = {false};
    for (int y = 0; y < kChunkHeight; ++y) {
      for (int x = 0; x < kChunkWidth; ++x) {
        if (mask[x][y]) continue;

        BlockType type = chunk.at(x, y, z);
        if (type == BlockType::AIR) continue;
        if (isLiquid(type)) continue; // Liquids only render top face

        BlockType neighbor = getBlock(world, chunk, x, y, z - 1);
        if (shouldRenderFace(type, neighbor)) {
          const auto &tex = block_data.at(type);

          int width = 1;
          while (x + width < kChunkWidth && !mask[x + width][y] &&
                 chunk.safeAt(x + width, y, z) == type &&
                 shouldRenderFace(type, getBlock(world, chunk, x + width, y, z - 1))) {
            width++;
          }

          int height = 1;
          bool can_expand = true;
          while (y + height < kChunkHeight && can_expand) {
            for (int i = 0; i < width; ++i) {
              if (mask[x + i][y + height] ||
                  chunk.safeAt(x + i, y + height, z) != type ||
                  !shouldRenderFace(type, getBlock(world, chunk, x + i, y + height, z - 1))) {
                can_expand = false;
                break;
              }
            }
            if (can_expand) height++;
          }

          for (int i = 0; i < height; ++i)
            for (int j = 0; j < width; ++j)
              mask[x + j][y + i] = true;

          addQuad(buffers, x, y, z, width, height,
                  tex.side.x, tex.side.y, Face::Back, chunkWorldX, chunkWorldZ,
                  isTransparent(type));
        }
      }
    }
  }

  // Top face (+Y)
  for (int y = 0; y < kChunkHeight; ++y) {
    bool mask[kChunkDepth][kChunkWidth] = {false};
    for (int z = 0; z < kChunkDepth; ++z) {
      for (int x = 0; x < kChunkWidth; ++x) {
        if (mask[z][x]) continue;

        BlockType type = chunk.at(x, y, z);
        if (type == BlockType::AIR) continue;

        BlockType neighbor = getBlock(world, chunk, x, y + 1, z);
        if (shouldRenderFace(type, neighbor)) {
          const auto &tex = block_data.at(type);

          int width = 1;
          while (x + width < kChunkWidth && !mask[z][x + width] &&
                 chunk.safeAt(x + width, y, z) == type &&
                 shouldRenderFace(type, getBlock(world, chunk, x + width, y + 1, z))) {
            width++;
          }

          int depth = 1;
          bool can_expand = true;
          while (z + depth < kChunkDepth && can_expand) {
            for (int i = 0; i < width; ++i) {
              if (mask[z + depth][x + i] ||
                  chunk.safeAt(x + i, y, z + depth) != type ||
                  !shouldRenderFace(type, getBlock(world, chunk, x + i, y + 1, z + depth))) {
                can_expand = false;
                break;
              }
            }
            if (can_expand) depth++;
          }

          for (int i = 0; i < depth; ++i)
            for (int j = 0; j < width; ++j)
              mask[z + i][x + j] = true;

          addQuad(buffers, x, y, z, width, depth,
                  tex.top.x, tex.top.y, Face::Top, chunkWorldX, chunkWorldZ,
                  isTransparent(type));
        }
      }
    }
  }

  // Bottom face (-Y)
  for (int y = kChunkHeight - 1; y >= 0; --y) {
    bool mask[kChunkDepth][kChunkWidth] = {false};
    for (int z = 0; z < kChunkDepth; ++z) {
      for (int x = 0; x < kChunkWidth; ++x) {
        if (mask[z][x]) continue;

        BlockType type = chunk.at(x, y, z);
        if (type == BlockType::AIR) continue;
        if (isLiquid(type)) continue; // Liquids only render top face

        BlockType neighbor = getBlock(world, chunk, x, y - 1, z);
        if (shouldRenderFace(type, neighbor)) {
          const auto &tex = block_data.at(type);

          int width = 1;
          while (x + width < kChunkWidth && !mask[z][x + width] &&
                 chunk.safeAt(x + width, y, z) == type &&
                 shouldRenderFace(type, getBlock(world, chunk, x + width, y - 1, z))) {
            width++;
          }

          int depth = 1;
          bool can_expand = true;
          while (z + depth < kChunkDepth && can_expand) {
            for (int i = 0; i < width; ++i) {
              if (mask[z + depth][x + i] ||
                  chunk.safeAt(x + i, y, z + depth) != type ||
                  !shouldRenderFace(type, getBlock(world, chunk, x + i, y - 1, z + depth))) {
                can_expand = false;
                break;
              }
            }
            if (can_expand) depth++;
          }

          for (int i = 0; i < depth; ++i)
            for (int j = 0; j < width; ++j)
              mask[z + i][x + j] = true;

          addQuad(buffers, x, y, z, width, depth,
                  tex.bottom.x, tex.bottom.y, Face::Bottom, chunkWorldX, chunkWorldZ,
                  isTransparent(type));
        }
      }
    }
  }

  // Right face (+X)
  for (int x = 0; x < kChunkWidth; ++x) {
    bool mask[kChunkHeight][kChunkDepth] = {false};
    for (int y = 0; y < kChunkHeight; ++y) {
      for (int z = 0; z < kChunkDepth; ++z) {
        if (mask[y][z]) continue;

        BlockType type = chunk.at(x, y, z);
        if (type == BlockType::AIR) continue;
        if (isLiquid(type)) continue; // Liquids only render top face

        BlockType neighbor = getBlock(world, chunk, x + 1, y, z);
        if (shouldRenderFace(type, neighbor)) {
          const auto &tex = block_data.at(type);

          int depth = 1;
          while (z + depth < kChunkDepth && !mask[y][z + depth] &&
                 chunk.safeAt(x, y, z + depth) == type &&
                 shouldRenderFace(type, getBlock(world, chunk, x + 1, y, z + depth))) {
            depth++;
          }

          int height = 1;
          bool can_expand = true;
          while (y + height < kChunkHeight && can_expand) {
            for (int i = 0; i < depth; ++i) {
              if (mask[y + height][z + i] ||
                  chunk.safeAt(x, y + height, z + i) != type ||
                  !shouldRenderFace(type, getBlock(world, chunk, x + 1, y + height, z + i))) {
                can_expand = false;
                break;
              }
            }
            if (can_expand) height++;
          }

          for (int i = 0; i < height; ++i)
            for (int j = 0; j < depth; ++j)
              mask[y + i][z + j] = true;

          addQuad(buffers, x, y, z, depth, height,
                  tex.side.x, tex.side.y, Face::Right, chunkWorldX, chunkWorldZ,
                  isTransparent(type));
        }
      }
    }
  }

  // Left face (-X)
  for (int x = kChunkWidth - 1; x >= 0; --x) {
    bool mask[kChunkHeight][kChunkDepth] = {false};
    for (int y = 0; y < kChunkHeight; ++y) {
      for (int z = 0; z < kChunkDepth; ++z) {
        if (mask[y][z]) continue;

        BlockType type = chunk.at(x, y, z);
        if (type == BlockType::AIR) continue;
        if (isLiquid(type)) continue; // Liquids only render top face

        BlockType neighbor = getBlock(world, chunk, x - 1, y, z);
        if (shouldRenderFace(type, neighbor)) {
          const auto &tex = block_data.at(type);

          int depth = 1;
          while (z + depth < kChunkDepth && !mask[y][z + depth] &&
                 chunk.safeAt(x, y, z + depth) == type &&
                 shouldRenderFace(type, getBlock(world, chunk, x - 1, y, z + depth))) {
            depth++;
          }

          int height = 1;
          bool can_expand = true;
          while (y + height < kChunkHeight && can_expand) {
            for (int i = 0; i < depth; ++i) {
              if (mask[y + height][z + i] ||
                  chunk.safeAt(x, y + height, z + i) != type ||
                  !shouldRenderFace(type, getBlock(world, chunk, x - 1, y + height, z + i))) {
                can_expand = false;
                break;
              }
            }
            if (can_expand) height++;
          }

          for (int i = 0; i < height; ++i)
            for (int j = 0; j < depth; ++j)
              mask[y + i][z + j] = true;

          addQuad(buffers, x, y, z, depth, height,
                  tex.side.x, tex.side.y, Face::Left, chunkWorldX, chunkWorldZ,
                  isTransparent(type));
        }
      }
    }
  }

  cpuReady = !vertices.empty() || !transparentVertices.empty();
}

void ChunkMesh::setupVAO(GLuint vao, GLuint vbo, GLuint ebo,
                          const std::vector<BlockVertex>& verts,
                          const std::vector<unsigned int>& inds) {
  glBindVertexArray(vao);

  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(BlockVertex),
               verts.data(), GL_STATIC_DRAW);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, inds.size() * sizeof(unsigned int),
               inds.data(), GL_STATIC_DRAW);

  // Position: 3 x int16 at offset 0
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_SHORT, GL_FALSE, sizeof(BlockVertex),
                        (void *)0);

  // FaceId: uint8 at offset 6
  glEnableVertexAttribArray(1);
  glVertexAttribIPointer(1, 1, GL_UNSIGNED_BYTE, sizeof(BlockVertex),
                         (void *)offsetof(BlockVertex, faceId));

  // TileXY: 2 x uint8 at offset 7
  glEnableVertexAttribArray(2);
  glVertexAttribIPointer(2, 2, GL_UNSIGNED_BYTE, sizeof(BlockVertex),
                         (void *)offsetof(BlockVertex, tileX));

  // UV: 2 x uint8 at offset 9
  glEnableVertexAttribArray(3);
  glVertexAttribIPointer(3, 2, GL_UNSIGNED_BYTE, sizeof(BlockVertex),
                         (void *)offsetof(BlockVertex, uvX));

  // ChunkOffset: 2 x int16 at offset 12
  glEnableVertexAttribArray(4);
  glVertexAttribIPointer(4, 2, GL_SHORT, sizeof(BlockVertex),
                         (void *)offsetof(BlockVertex, chunkX));

  glBindVertexArray(0);
}

void ChunkMesh::upload() {
  if (!cpuReady)
    return;

  if (!vertices.empty()) {
    if (!VAO) {
      glGenVertexArrays(1, &VAO);
      glGenBuffers(1, &VBO);
      glGenBuffers(1, &EBO);
    }
    setupVAO(VAO, VBO, EBO, vertices, indices);
  }

  if (!transparentVertices.empty()) {
    if (!transparentVAO) {
      glGenVertexArrays(1, &transparentVAO);
      glGenBuffers(1, &transparentVBO);
      glGenBuffers(1, &transparentEBO);
    }
    setupVAO(transparentVAO, transparentVBO, transparentEBO,
             transparentVertices, transparentIndices);
  }

  gpuUploaded = true;
}

void ChunkMesh::renderOpaque() {
  if (!gpuUploaded || indices.empty())
    return;

  glBindVertexArray(VAO);
  glDrawElements(GL_TRIANGLES, (GLsizei)indices.size(), GL_UNSIGNED_INT, 0);
  glBindVertexArray(0);
}

void ChunkMesh::renderTransparent() {
  if (!gpuUploaded || transparentIndices.empty())
    return;

  glBindVertexArray(transparentVAO);
  glDrawElements(GL_TRIANGLES, (GLsizei)transparentIndices.size(), GL_UNSIGNED_INT, 0);
  glBindVertexArray(0);
}
