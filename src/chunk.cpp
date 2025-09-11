#include "chunk.hpp"
#include "block.hpp"
#include "block_type.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/noise.hpp>

Chunk::Chunk(const int w, const int l, const int h, const Texture &atlas,
             const int world_x, const int world_z)
    : width(w), length(l), world_x(world_x), world_z(world_z) {
  mesh.textures.push_back(atlas);

  for (int x = 0; x < width; x++) {
    for (int z = 0; z < length; z++) {
      const double height_noise = glm::perlin(
          glm::vec2((world_x * width + x) * 0.01, (world_z * length + z) * 0.01));
      height = static_cast<int>(height_noise * h) + h;

      for (int y = 0; y < height; y++) {
        BlockType type = BlockType::AIR;
        const double stone_noise = glm::perlin(glm::vec3(
            (world_x * width + x) * 0.55, y * 0.25, (world_z * length + z) * 0.25));
        const double bedrock_noise = glm::perlin(glm::vec3(
            (world_x * width + x) * 0.05, y * 0.05, (world_z * length + z) * 0.05));

        if (y == height - 1) {
          type = BlockType::GRASS;
        } else if ((bedrock_noise > 0.1 && y >= 0 && y <= 10) || (y >= 0 && y < 5)) {
          type = BlockType::BEDROCK;
        } else if ((stone_noise > 0.4) || (y >= 5 && y <= 15)) {
          type = BlockType::STONE;
        } else {
          type = BlockType::DIRT;
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
