#include "chunk/chunk_mesh.hpp"
#include "block/block_texture.hpp"
#include "chunk/chunk.hpp"
#include "core/constants.hpp"

#include <glm/gtc/matrix_transform.hpp>

namespace {

void addQuad(std::vector<BlockVertex> &vertices, std::vector<unsigned int> &indices, const glm::ivec3 &pos, const glm::ivec2 &size, const std::array<glm::vec2, 4> &uvs, Face face);

void addQuad(std::vector<BlockVertex> &vertices,
             std::vector<unsigned int> &indices,
             const glm::ivec3 &pos,
             const glm::ivec2 &size,
             const std::array<glm::vec2, 4> &uvs,
             Face face) {
  int start_index = vertices.size();

  switch (face) {
    case Face::Top: // +Y
      vertices.push_back({{pos.x - 0.5f, pos.y + 0.5f, pos.z - 0.5f}, {0,1,0}, uvs[0]}); // TL
      vertices.push_back({{pos.x + size.x - 0.5f, pos.y + 0.5f, pos.z - 0.5f}, {0,1,0}, uvs[1]}); // TR
      vertices.push_back({{pos.x + size.x - 0.5f, pos.y + 0.5f, pos.z + size.y - 0.5f}, {0,1,0}, uvs[2]}); // BR
      vertices.push_back({{pos.x - 0.5f, pos.y + 0.5f, pos.z + size.y - 0.5f}, {0,1,0}, uvs[3]}); // BL
      break;

    case Face::Bottom: // -Y
      vertices.push_back({{pos.x - 0.5f, pos.y - 0.5f, pos.z + size.y - 0.5f}, {0,-1,0}, uvs[0]}); // TL
      vertices.push_back({{pos.x + size.x - 0.5f, pos.y - 0.5f, pos.z + size.y - 0.5f}, {0,-1,0}, uvs[1]}); // TR
      vertices.push_back({{pos.x + size.x - 0.5f, pos.y - 0.5f, pos.z - 0.5f}, {0,-1,0}, uvs[2]}); // BR
      vertices.push_back({{pos.x - 0.5f, pos.y - 0.5f, pos.z - 0.5f}, {0,-1,0}, uvs[3]}); // BL
      break;

    case Face::Front: // +Z
      vertices.push_back({{pos.x - 0.5f, pos.y + size.y - 0.5f, pos.z + 0.5f}, {0,0,1}, uvs[0]}); // TL
      vertices.push_back({{pos.x + size.x - 0.5f, pos.y + size.y - 0.5f, pos.z + 0.5f}, {0,0,1}, uvs[1]}); // TR
      vertices.push_back({{pos.x + size.x - 0.5f, pos.y - 0.5f, pos.z + 0.5f}, {0,0,1}, uvs[2]}); // BR
      vertices.push_back({{pos.x - 0.5f, pos.y - 0.5f, pos.z + 0.5f}, {0,0,1}, uvs[3]}); // BL
      break;
    case Face::Back: // -Z
      vertices.push_back({{pos.x + size.x - 0.5f, pos.y + size.y - 0.5f, pos.z - 0.5f}, {0,0,-1}, uvs[0]}); // TL
      vertices.push_back({{pos.x - 0.5f, pos.y + size.y - 0.5f, pos.z - 0.5f}, {0,0,-1}, uvs[1]}); // TR
      vertices.push_back({{pos.x - 0.5f, pos.y - 0.5f, pos.z - 0.5f}, {0,0,-1}, uvs[2]}); // BR
      vertices.push_back({{pos.x + size.x - 0.5f, pos.y - 0.5f, pos.z - 0.5f}, {0,0,-1}, uvs[3]}); // BL
      break;

    case Face::Right: // +X
      vertices.push_back({{pos.x + 0.5f, pos.y + size.y - 0.5f, pos.z - 0.5f}, {1,0,0}, uvs[0]}); // TL
      vertices.push_back({{pos.x + 0.5f, pos.y + size.y - 0.5f, pos.z + size.x - 0.5f}, {1,0,0}, uvs[1]}); // TR
      vertices.push_back({{pos.x + 0.5f, pos.y - 0.5f, pos.z + size.x - 0.5f}, {1,0,0}, uvs[2]}); // BR
      vertices.push_back({{pos.x + 0.5f, pos.y - 0.5f, pos.z - 0.5f}, {1,0,0}, uvs[3]}); // BL
      break;

    case Face::Left: // -X
      vertices.push_back({{pos.x - 0.5f, pos.y + size.y - 0.5f, pos.z + size.x - 0.5f}, {-1,0,0}, uvs[0]}); // TL
      vertices.push_back({{pos.x - 0.5f, pos.y + size.y - 0.5f, pos.z - 0.5f}, {-1,0,0}, uvs[1]}); // TR
      vertices.push_back({{pos.x - 0.5f, pos.y - 0.5f, pos.z - 0.5f}, {-1,0,0}, uvs[2]}); // BR
      vertices.push_back({{pos.x - 0.5f, pos.y - 0.5f, pos.z + size.x - 0.5f}, {-1,0,0}, uvs[3]}); // BL
      break;
  }

  // per-face CCW indices (matching TL,TR,BR,BL vertex order above)
  switch (face) {
    // these four need the diagonal the other way
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

    // left/right are already CCW with the current vertex order
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
  glGenVertexArrays(1, &VAO);
  glGenBuffers(1, &VBO);
  glGenBuffers(1, &EBO);
}

ChunkMesh::~ChunkMesh() {
  glDeleteVertexArrays(1, &VAO);
  glDeleteBuffers(1, &VBO);
  glDeleteBuffers(1, &EBO);
}

void ChunkMesh::generate(Chunk &chunk, TextureManager &texture_manager) {
  vertices.clear();
  indices.clear();

  // Front face
  {
    // mask of visited blocks
    bool mask[kChunkHeight][kChunkWidth] = {false};

    for (int z = 0; z < kChunkDepth; ++z) {
      for (int y = 0; y < kChunkHeight; ++y) {
        for (int x = 0; x < kChunkWidth; ++x) {
          if (mask[y][x])
            continue;

          BlockType type = chunk.at(x, y, z);
          if (type == BlockType::AIR)
            continue;

          // check if face is visible
          if (z == kChunkDepth - 1 || chunk.at(x, y, z + 1) == BlockType::AIR) {
            const auto &tex = block_uv_map.at(type);
            auto side_uv = texture_manager.getQuadUV(tex.side.x, tex.side.y);

            // greedy expansion in width
            int width = 1;
            while (x + width < kChunkWidth && !mask[y][x + width] && chunk.at(x + width, y, z) == type && (z == kChunkDepth - 1 || chunk.at(x + width, y, z + 1) == BlockType::AIR)) {
              width++;
            }

            // greedy expansion in height
            int height = 1;
            bool can_expand_height = true;
            while (y + height < kChunkHeight && can_expand_height) {
              for (int i = 0; i < width; ++i) {
                if (mask[y + height][x + i] || chunk.at(x + i, y + height, z) != type || (z != kChunkDepth - 1 && chunk.at(x + i, y + height, z + 1) != BlockType::AIR)) {
                  can_expand_height = false;
                  break;
                }
              }
              if (can_expand_height) {
                height++;
              }
            }

            // mark expanded blocks as visited
            for (int i = 0; i < height; ++i) {
              for (int j = 0; j < width; ++j) {
                mask[y + i][x + j] = true;
              }
            }

            addQuad(vertices, indices, {x, y, z}, {width, height}, side_uv, Face::Front);
          }
        }
      }
    }
  }

  // Back face
  {
    // mask of visited blocks
    bool mask[kChunkHeight][kChunkWidth] = {false};

    for (int z = kChunkDepth - 1; z >= 0; --z) {
      for (int y = 0; y < kChunkHeight; ++y) {
        for (int x = 0; x < kChunkWidth; ++x) {
          if (mask[y][x])
            continue;

          BlockType type = chunk.at(x, y, z);
          if (type == BlockType::AIR)
            continue;

          // check if face is visible
          if (z == 0 || chunk.at(x, y, z - 1) == BlockType::AIR) {
            const auto &tex = block_uv_map.at(type);
            auto side_uv = texture_manager.getQuadUV(tex.side.x, tex.side.y);

            // greedy expansion in width
            int width = 1;
            while (x + width < kChunkWidth && !mask[y][x + width] && chunk.at(x + width, y, z) == type && (z == 0 || chunk.at(x + width, y, z - 1) == BlockType::AIR)) {
              width++;
            }

            // greedy expansion in height
            int height = 1;
            bool can_expand_height = true;
            while (y + height < kChunkHeight && can_expand_height) {
              for (int i = 0; i < width; ++i) {
                if (mask[y + height][x + i] || chunk.at(x + i, y + height, z) != type || (z != 0 && chunk.at(x + i, y + height, z - 1) != BlockType::AIR)) {
                  can_expand_height = false;
                  break;
                }
              }
              if (can_expand_height) {
                height++;
              }
            }

            // mark expanded blocks as visited
            for (int i = 0; i < height; ++i) {
              for (int j = 0; j < width; ++j) {
                mask[y + i][x + j] = true;
              }
            }

            addQuad(vertices, indices, {x, y, z}, {width, height}, side_uv, Face::Back);
          }
        }
      }
    }
  }

  // Top face
  {
    // mask of visited blocks
    bool mask[kChunkDepth][kChunkWidth] = {false};

    for (int y = 0; y < kChunkHeight; ++y) {
      for (int z = 0; z < kChunkDepth; ++z) {
        for (int x = 0; x < kChunkWidth; ++x) {
          if (mask[z][x])
            continue;

          BlockType type = chunk.at(x, y, z);
          if (type == BlockType::AIR)
            continue;

          // check if face is visible
          if (y == kChunkHeight - 1 || chunk.at(x, y + 1, z) == BlockType::AIR) {
            const auto &tex = block_uv_map.at(type);
            auto top_uv = texture_manager.getQuadUV(tex.top.x, tex.top.y);

            // greedy expansion in width
            int width = 1;
            while (x + width < kChunkWidth && !mask[z][x + width] && chunk.at(x + width, y, z) == type && (y == kChunkHeight - 1 || chunk.at(x + width, y + 1, z) == BlockType::AIR)) {
              width++;
            }

            // greedy expansion in depth
            int depth = 1;
            bool can_expand_depth = true;
            while (z + depth < kChunkDepth && can_expand_depth) {
              for (int i = 0; i < width; ++i) {
                if (mask[z + depth][x + i] || chunk.at(x + i, y, z + depth) != type || (y != kChunkHeight - 1 && chunk.at(x + i, y + 1, z + depth) != BlockType::AIR)) {
                  can_expand_depth = false;
                  break;
                }
              }
              if (can_expand_depth) {
                depth++;
              }
            }

            // mark expanded blocks as visited
            for (int i = 0; i < depth; ++i) {
              for (int j = 0; j < width; ++j) {
                mask[z + i][x + j] = true;
              }
            }

            addQuad(vertices, indices, {x, y, z}, {width, depth}, top_uv, Face::Top);
          }
        }
      }
    }
  }

  // Bottom face
  {
    // mask of visited blocks
    bool mask[kChunkDepth][kChunkWidth] = {false};

    for (int y = kChunkHeight - 1; y >= 0; --y) {
      for (int z = 0; z < kChunkDepth; ++z) {
        for (int x = 0; x < kChunkWidth; ++x) {
          if (mask[z][x])
            continue;

          BlockType type = chunk.at(x, y, z);
          if (type == BlockType::AIR)
            continue;

          // check if face is visible
          if (y == 0 || chunk.at(x, y - 1, z) == BlockType::AIR) {
            const auto &tex = block_uv_map.at(type);
            auto bottom_uv = texture_manager.getQuadUV(tex.bottom.x, tex.bottom.y);

            // greedy expansion in width
            int width = 1;
            while (x + width < kChunkWidth && !mask[z][x + width] && chunk.at(x + width, y, z) == type && (y == 0 || chunk.at(x + width, y - 1, z) == BlockType::AIR)) {
              width++;
            }

            // greedy expansion in depth
            int depth = 1;
            bool can_expand_depth = true;
            while (z + depth < kChunkDepth && can_expand_depth) {
              for (int i = 0; i < width; ++i) {
                if (mask[z + depth][x + i] || chunk.at(x + i, y, z + depth) != type || (y != 0 && chunk.at(x + i, y - 1, z + depth) != BlockType::AIR)) {
                  can_expand_depth = false;
                  break;
                }
              }
              if (can_expand_depth) {
                depth++;
              }
            }

            // mark expanded blocks as visited
            for (int i = 0; i < depth; ++i) {
              for (int j = 0; j < width; ++j) {
                mask[z + i][x + j] = true;
              }
            }

            addQuad(vertices, indices, {x, y, z}, {width, depth}, bottom_uv, Face::Bottom);
          }
        }
      }
    }
  }

  // Right face
  {
    // mask of visited blocks
    bool mask[kChunkHeight][kChunkDepth] = {false};

    for (int x = 0; x < kChunkWidth; ++x) {
      for (int y = 0; y < kChunkHeight; ++y) {
        for (int z = 0; z < kChunkDepth; ++z) {
          if (mask[y][z])
            continue;

          BlockType type = chunk.at(x, y, z);
          if (type == BlockType::AIR)
            continue;

          // check if face is visible
          if (x == kChunkWidth - 1 || chunk.at(x + 1, y, z) == BlockType::AIR) {
            const auto &tex = block_uv_map.at(type);
            auto side_uv = texture_manager.getQuadUV(tex.side.x, tex.side.y);

            // greedy expansion in depth
            int depth = 1;
            while (z + depth < kChunkDepth && !mask[y][z + depth] && chunk.at(x, y, z + depth) == type && (x == kChunkWidth - 1 || chunk.at(x + 1, y, z + depth) == BlockType::AIR)) {
              depth++;
            }

            // greedy expansion in height
            int height = 1;
            bool can_expand_height = true;
            while (y + height < kChunkHeight && can_expand_height) {
              for (int i = 0; i < depth; ++i) {
                if (mask[y + height][z + i] || chunk.at(x, y + height, z + i) != type || (x != kChunkWidth - 1 && chunk.at(x + 1, y + height, z + i) != BlockType::AIR)) {
                  can_expand_height = false;
                  break;
                }
              }
              if (can_expand_height) {
                height++;
              }
            }

            // mark expanded blocks as visited
            for (int i = 0; i < height; ++i) {
              for (int j = 0; j < depth; ++j) {
                mask[y + i][z + j] = true;
              }
            }

            addQuad(vertices, indices, {x, y, z}, {depth, height}, side_uv, Face::Right);
          }
        }
      }
    }
  }

  // Left face
  {
    // mask of visited blocks
    bool mask[kChunkHeight][kChunkDepth] = {false};

    for (int x = kChunkWidth - 1; x >= 0; --x) {
      for (int y = 0; y < kChunkHeight; ++y) {
        for (int z = 0; z < kChunkDepth; ++z) {
          if (mask[y][z])
            continue;

          BlockType type = chunk.at(x, y, z);
          if (type == BlockType::AIR)
            continue;

          // check if face is visible
          if (x == 0 || chunk.at(x - 1, y, z) == BlockType::AIR) {
            const auto &tex = block_uv_map.at(type);
            auto side_uv = texture_manager.getQuadUV(tex.side.x, tex.side.y);

            // greedy expansion in depth
            int depth = 1;
            while (z + depth < kChunkDepth && !mask[y][z + depth] && chunk.at(x, y, z + depth) == type && (x == 0 || chunk.at(x - 1, y, z + depth) == BlockType::AIR)) {
              depth++;
            }

            // greedy expansion in height
            int height = 1;
            bool can_expand_height = true;
            while (y + height < kChunkHeight && can_expand_height) {
              for (int i = 0; i < depth; ++i) {
                if (mask[y + height][z + i] || chunk.at(x, y + height, z + i) != type || (x != 0 && chunk.at(x - 1, y + height, z + i) != BlockType::AIR)) {
                  can_expand_height = false;
                  break;
                }
              }
              if (can_expand_height) {
                height++;
              }
            }

            // mark expanded blocks as visited
            for (int i = 0; i < height; ++i) {
              for (int j = 0; j < depth; ++j) {
                mask[y + i][z + j] = true;
              }
            }

            addQuad(vertices, indices, {x, y, z}, {depth, height}, side_uv, Face::Left);
          }
        }
      }
    }
  }
}

void ChunkMesh::upload() {
  glBindVertexArray(VAO);

  glBindBuffer(GL_ARRAY_BUFFER, VBO);
  glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(BlockVertex),
               vertices.data(), GL_STATIC_DRAW);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int),
               indices.data(), GL_STATIC_DRAW);

  // Position
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(BlockVertex),
                        (void *)0);

  // Normal
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(BlockVertex),
                        (void *)offsetof(BlockVertex, normal));

  // UV
  glEnableVertexAttribArray(2);
  glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(BlockVertex),
                        (void *)offsetof(BlockVertex, uv));

  glBindVertexArray(0);
}

void ChunkMesh::render() {
  if (vertices.empty())
    return;

  glBindVertexArray(VAO);
  glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_INT, 0);
  glBindVertexArray(0);
}
