#include "chunk/chunk_mesh.hpp"
#include "block/block_texture.hpp"
#include "chunk/chunk.hpp"
#include "core/constants.hpp"

#include <glm/gtc/matrix_transform.hpp>

namespace {

void addFace(std::vector<BlockVertex> &vertices,
             std::vector<unsigned int> &indices,
             int x, int y, int z,
             const std::array<glm::vec2, 4> &uvs,
             Face face) {
  int start_index = vertices.size();

  switch (face) {
    case Face::Top: // +Y
      vertices.push_back({{x - 0.5f, y + 0.5f, z - 0.5f}, {0,1,0}, uvs[0]}); // TL
      vertices.push_back({{x + 0.5f, y + 0.5f, z - 0.5f}, {0,1,0}, uvs[1]}); // TR
      vertices.push_back({{x + 0.5f, y + 0.5f, z + 0.5f}, {0,1,0}, uvs[2]}); // BR
      vertices.push_back({{x - 0.5f, y + 0.5f, z + 0.5f}, {0,1,0}, uvs[3]}); // BL
      break;

    case Face::Bottom: // -Y
      vertices.push_back({{x - 0.5f, y - 0.5f, z + 0.5f}, {0,-1,0}, uvs[0]}); // TL
      vertices.push_back({{x + 0.5f, y - 0.5f, z + 0.5f}, {0,-1,0}, uvs[1]}); // TR
      vertices.push_back({{x + 0.5f, y - 0.5f, z - 0.5f}, {0,-1,0}, uvs[2]}); // BR
      vertices.push_back({{x - 0.5f, y - 0.5f, z - 0.5f}, {0,-1,0}, uvs[3]}); // BL
      break;

    case Face::Front: // +Z
      vertices.push_back({{x - 0.5f, y + 0.5f, z + 0.5f}, {0,0,1}, uvs[0]}); // TL
      vertices.push_back({{x + 0.5f, y + 0.5f, z + 0.5f}, {0,0,1}, uvs[1]}); // TR
      vertices.push_back({{x + 0.5f, y - 0.5f, z + 0.5f}, {0,0,1}, uvs[2]}); // BR
      vertices.push_back({{x - 0.5f, y - 0.5f, z + 0.5f}, {0,0,1}, uvs[3]}); // BL
      break;

    case Face::Back: // -Z
      vertices.push_back({{x + 0.5f, y + 0.5f, z - 0.5f}, {0,0,-1}, uvs[0]}); // TL
      vertices.push_back({{x - 0.5f, y + 0.5f, z - 0.5f}, {0,0,-1}, uvs[1]}); // TR
      vertices.push_back({{x - 0.5f, y - 0.5f, z - 0.5f}, {0,0,-1}, uvs[2]}); // BR
      vertices.push_back({{x + 0.5f, y - 0.5f, z - 0.5f}, {0,0,-1}, uvs[3]}); // BL
      break;

    case Face::Right: // +X
      vertices.push_back({{x + 0.5f, y + 0.5f, z - 0.5f}, {1,0,0}, uvs[0]}); // TL
      vertices.push_back({{x + 0.5f, y + 0.5f, z + 0.5f}, {1,0,0}, uvs[1]}); // TR
      vertices.push_back({{x + 0.5f, y - 0.5f, z + 0.5f}, {1,0,0}, uvs[2]}); // BR
      vertices.push_back({{x + 0.5f, y - 0.5f, z - 0.5f}, {1,0,0}, uvs[3]}); // BL
      break;

    case Face::Left: // -X
      vertices.push_back({{x - 0.5f, y + 0.5f, z + 0.5f}, {-1,0,0}, uvs[0]}); // TL
      vertices.push_back({{x - 0.5f, y + 0.5f, z - 0.5f}, {-1,0,0}, uvs[1]}); // TR
      vertices.push_back({{x - 0.5f, y - 0.5f, z - 0.5f}, {-1,0,0}, uvs[2]}); // BR
      vertices.push_back({{x - 0.5f, y - 0.5f, z + 0.5f}, {-1,0,0}, uvs[3]}); // BL
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

  // FIXME: not a good idea here
  for (int y = 0; y < kChunkHeight; ++y) {
    for (int z = 0; z < kChunkDepth; ++z) {
      for (int x = 0; x < kChunkWidth; ++x) {
        BlockType type = chunk.at(x, y, z);
        if (type == BlockType::AIR)
          continue;

        const auto &tex = block_uv_map.at(type);

        // Top face
        if (y == kChunkHeight - 1 || chunk.at(x, y + 1, z) == BlockType::AIR) {
          auto top_uv = texture_manager.getQuadUV(tex.top.x, tex.top.y);
          addFace(vertices, indices, x, y, z, top_uv, Face::Top);
        }

        // Bottom face
        if (y == 0 || chunk.at(x, y - 1, z) == BlockType::AIR) {
          auto bottom_uv = texture_manager.getQuadUV(tex.bottom.x, tex.bottom.y);
          addFace(vertices, indices, x, y, z, bottom_uv, Face::Bottom);
        }

        // Left face
        if (x == 0 || chunk.at(x - 1, y, z) == BlockType::AIR) {
          auto side_uv = texture_manager.getQuadUV(tex.side.x, tex.side.y);
          addFace(vertices, indices, x, y, z, side_uv, Face::Left);
        }

        // Right face
        if (x == kChunkWidth - 1 || chunk.at(x + 1, y, z) == BlockType::AIR) {
          auto side_uv = texture_manager.getQuadUV(tex.side.x, tex.side.y);
          addFace(vertices, indices, x, y, z, side_uv, Face::Right);
        }

        // Front face
        if (z == kChunkDepth - 1 || chunk.at(x, y, z + 1) == BlockType::AIR) {
          auto side_uv = texture_manager.getQuadUV(tex.side.x, tex.side.y);
          addFace(vertices, indices, x, y, z, side_uv, Face::Front);
        }

        // Back face
        if (z == 0 || chunk.at(x, y, z - 1) == BlockType::AIR) {
          auto side_uv = texture_manager.getQuadUV(tex.side.x, tex.side.y);
          addFace(vertices, indices, x, y, z, side_uv, Face::Back);
        }
      }
    }
  }

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
