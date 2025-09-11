#include "chunk.hpp"
#include "block.hpp"
#include "block_type.hpp"
#include "perlin_noise.hpp"
#include <glm/glm.hpp>

Chunk::Chunk(const int w, const int l, const Texture &atlas)
    : width(w), length(l) {
  mesh.textures.push_back(atlas);

  const std::uint32_t seed = 123456u;
  const siv::PerlinNoise perlin{seed};

  for (int x = 0; x < width; x++) {
    for (int z = 0; z < length; z++) {
      const double noise = perlin.octaveNoise0_1(x * 0.01, z * 0.01, 4);
      height = static_cast<int>(noise * 64) + 64;

      for (int y = 0; y < height; y++) {
        BlockType type = BlockType::AIR;
        if (y == height - 1) {
          type = BlockType::GRASS;
        } else if (y > height - 5) {
          type = BlockType::DIRT;
        } else {
          type = BlockType::STONE;
        }

        if (type != BlockType::AIR) {
          Block block(type, glm::vec3(x, y, z));
          blocks.push_back(block);

          unsigned int offset = mesh.vertices.size();
          mesh.vertices.insert(mesh.vertices.end(), block.mesh.vertices.begin(),
                               block.mesh.vertices.end());

          for (auto index : block.mesh.indices) {
            mesh.indices.push_back(index + offset);
          }
        }
      }
    }
  }

  mesh.setupMesh();
}

Chunk::~Chunk() {}

void Chunk::draw(Shader &shader) { mesh.draw(shader); }
