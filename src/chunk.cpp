#include "chunk.hpp"
#include "block_type.hpp"
#include <glm/glm.hpp>

Chunk::Chunk(const Texture &atlas) {
  m_mesh.textures.push_back(atlas);

  for (int x = 0; x < CHUNK_WIDTH; x++) {
    for (int z = 0; z < CHUNK_LENGTH; z++) {
      for (int y = 0; y < CHUNK_HEIGHT; y++) {
        BlockType type = BlockType::AIR;
        if (y < 64) {
          type = BlockType::STONE;
        } else if (y < 128) {
          type = BlockType::DIRT;
        } else if (y == 128) {
          type = BlockType::GRASS;
        }

        if (type != BlockType::AIR) {
          Block block(type, glm::vec3(x, y, z));

          // This is a very inefficient way to merge meshes.
          // We are just inserting all vertices and indices from each block into
          // the chunk mesh.
          // We will need to optimize this later.
          unsigned int offset = m_mesh.vertices.size();
          m_mesh.vertices.insert(m_mesh.vertices.end(),
                                 block.mesh.vertices.begin(),
                                 block.mesh.vertices.end());

          for (auto index : block.mesh.indices) {
            m_mesh.indices.push_back(index + offset);
          }
        }
      }
    }
  }

  m_mesh.setupMesh();
}

Chunk::~Chunk() {}

void Chunk::draw(Shader &shader) { m_mesh.draw(shader); }