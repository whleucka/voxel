#include "cloud_manager.hpp"

#include <iostream>

#include "world_constants.hpp"
#include <glm/gtc/noise.hpp>

CloudManager::CloudManager() {
  m_width = 256;
  m_depth = 256;
  m_height = 128;
}

void CloudManager::generateCloudMesh() {
  std::vector<bool> cloud_map(m_width * m_depth, false);

  for (int x = 0; x < m_width; ++x) {
    for (int z = 0; z < m_depth; ++z) {
      float noise = glm::perlin(glm::vec2(x / 64.0f, z / 64.0f));
      if (noise > 0.6f) {
        cloud_map[x + z * m_width] = true;
      }
    }
  }

  // Greedy meshing
  for (int x = 0; x < m_width; ++x) {
    for (int z = 0; z < m_depth; ++z) {
      if (cloud_map[x + z * m_width]) {
        int w = 1, d = 1;

        while (x + w < m_width && cloud_map[(x + w) + z * m_width]) {
          w++;
        }

        while (z + d < m_depth) {
          bool solid_row = true;
          for (int i = 0; i < w; ++i) {
            if (!cloud_map[(x + i) + (z + d) * m_width]) {
              solid_row = false;
              break;
            }
          }
          if (solid_row) {
            d++;
          } else {
            break;
          }
        }

        // Add quad
        m_mesh.vertices.push_back({{ (float)x, (float)m_height, (float)z }, {0, 1, 0}, {0, 0}, {0, 0}, 1});
        m_mesh.vertices.push_back({{ (float)x + w, (float)m_height, (float)z }, {0, 1, 0}, {1, 0}, {0, 0}, 1});
        m_mesh.vertices.push_back({{ (float)x + w, (float)m_height, (float)z + d }, {0, 1, 0}, {1, 1}, {0, 0}, 1});
        m_mesh.vertices.push_back({{ (float)x, (float)m_height, (float)z + d }, {0, 1, 0}, {0, 1}, {0, 0}, 1});

        GLuint base_index = (m_mesh.vertices.size()) - 4;
        m_mesh.indices.insert(m_mesh.indices.end(), {base_index, base_index + 2, base_index + 1, base_index, base_index + 3, base_index + 2});

        for (int i = 0; i < w; ++i) {
          for (int j = 0; j < d; ++j) {
            cloud_map[(x + i) + (z + j) * m_width] = false;
          }
        }
      }
    }
  }

  m_mesh.setupMesh();
}

void CloudManager::render(Shader& shader) const {
  shader.use();
  m_mesh.draw(shader);
}
