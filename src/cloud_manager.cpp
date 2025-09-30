#include "cloud_manager.hpp"
#include "world_constants.hpp"
#include <climits>
#include <glm/gtc/noise.hpp>
#include <iostream>

CloudManager::CloudManager()
    : m_width(512), m_depth(512), m_height(CLOUD_LEVEL), renderDistance(3) {}

void CloudManager::generateChunk(int cx, int cz, Mesh& mesh) {
    int originX = cx * m_width;
    int originZ = cz * m_depth;

    std::vector<Vertex> verts;
    std::vector<unsigned int> inds;

    const float thickness = 4.0f;
    float topY    = (float)m_height;
    float bottomY = topY - thickness;

    int cellSize = 16;
    int cellsX = m_width / cellSize;
    int cellsZ = m_depth / cellSize;

    // Build coarse cloud map
    std::vector<uint8_t> cloud_map(cellsX * cellsZ, 0);
    for (int cxCell = 0; cxCell < cellsX; ++cxCell) {
        for (int czCell = 0; czCell < cellsZ; ++czCell) {
            float wx = originX + cxCell * cellSize;
            float wz = originZ + czCell * cellSize;

            float noise = glm::perlin(glm::vec2(wx / 128.0f, wz / 128.0f));
            float n = (noise + 1.0f) * 0.5f;

            if (n > 0.5f) {
                cloud_map[cxCell + czCell * cellsX] = 1;
            }
        }
    }

    auto isFilled = [&](int cxCell, int czCell) -> bool {
        if (cxCell < 0 || czCell < 0 || cxCell >= cellsX || czCell >= cellsZ)
            return false;
        return cloud_map[cxCell + czCell * cellsX] != 0;
    };

    // Emit geometry for visible faces only
    for (int cxCell = 0; cxCell < cellsX; ++cxCell) {
      for (int czCell = 0; czCell < cellsZ; ++czCell) {
        if (isFilled(cxCell, czCell)) {
          float wx = originX + cxCell * cellSize;
          float wz = originZ + czCell * cellSize;
          emitCloudCell(wx, bottomY, topY, wz, cellSize, verts, inds);
        }
      }
    }

    mesh.upload(verts, inds);
}

void CloudManager::emitCloudCell(float x, float yBottom, float yTop, float z,
                   float cellSize,
                   std::vector<Vertex>& verts,
                   std::vector<unsigned int>& inds) 
{
    unsigned int base;

    float x0 = x;
    float x1 = x + cellSize;
    float z0 = z;
    float z1 = z + cellSize;

    // --- TOP (+Y) ---
    base = verts.size();
    verts.push_back({{x0, yTop, z0}, {0,1,0}, {0,0}, {0,0}, 1.0f});
    verts.push_back({{x0, yTop, z1}, {0,1,0}, {0,1}, {0,0}, 1.0f});
    verts.push_back({{x1, yTop, z1}, {0,1,0}, {1,1}, {0,0}, 1.0f});
    verts.push_back({{x1, yTop, z0}, {0,1,0}, {1,0}, {0,0}, 1.0f});
    inds.insert(inds.end(), {base, base+1, base+2, base, base+2, base+3});

    // --- BOTTOM (-Y) ---
    base = verts.size();
    verts.push_back({{x0, yBottom, z0}, {0,-1,0}, {0,0}, {0,0}, 0.75f});
    verts.push_back({{x1, yBottom, z0}, {0,-1,0}, {1,0}, {0,0}, 0.75f});
    verts.push_back({{x1, yBottom, z1}, {0,-1,0}, {1,1}, {0,0}, 0.75f});
    verts.push_back({{x0, yBottom, z1}, {0,-1,0}, {0,1}, {0,0}, 0.75f});
    inds.insert(inds.end(), {base, base+1, base+2, base, base+2, base+3});

    // --- LEFT (-X) ---
    base = verts.size();
    verts.push_back({{x0, yBottom, z0}, {-1,0,0}, {0,0}, {0,0}, 0.85f});
    verts.push_back({{x0, yBottom, z1}, {-1,0,0}, {1,0}, {0,0}, 0.85f});
    verts.push_back({{x0, yTop,    z1}, {-1,0,0}, {1,1}, {0,0}, 0.85f});
    verts.push_back({{x0, yTop,    z0}, {-1,0,0}, {0,1}, {0,0}, 0.85f});
    inds.insert(inds.end(), {base, base+1, base+2, base, base+2, base+3});

    // --- RIGHT (+X) ---
    base = verts.size();
    verts.push_back({{x1, yBottom, z0}, {1,0,0}, {0,0}, {0,0}, 0.85f});
    verts.push_back({{x1, yTop,    z0}, {1,0,0}, {0,1}, {0,0}, 0.85f});
    verts.push_back({{x1, yTop,    z1}, {1,0,0}, {1,1}, {0,0}, 0.85f});
    verts.push_back({{x1, yBottom, z1}, {1,0,0}, {1,0}, {0,0}, 0.85f});
    inds.insert(inds.end(), {base, base+1, base+2, base, base+2, base+3});

    // --- FRONT (-Z) ---
    base = verts.size();
    verts.push_back({{x0, yBottom, z0}, {0,0,-1}, {0,0}, {0,0}, 0.85f});
    verts.push_back({{x0, yTop,    z0}, {0,0,-1}, {0,1}, {0,0}, 0.85f});
    verts.push_back({{x1, yTop,    z0}, {0,0,-1}, {1,1}, {0,0}, 0.85f});
    verts.push_back({{x1, yBottom, z0}, {0,0,-1}, {1,0}, {0,0}, 0.85f});
    inds.insert(inds.end(), {base, base+1, base+2, base, base+2, base+3});

    // --- BACK (+Z) ---
    base = verts.size();
    verts.push_back({{x0, yBottom, z1}, {0,0,1}, {0,0}, {0,0}, 0.85f});
    verts.push_back({{x1, yBottom, z1}, {0,0,1}, {1,0}, {0,0}, 0.85f});
    verts.push_back({{x1, yTop,    z1}, {0,0,1}, {1,1}, {0,0}, 0.85f});
    verts.push_back({{x0, yTop,    z1}, {0,0,1}, {0,1}, {0,0}, 0.85f});
    inds.insert(inds.end(), {base, base+1, base+2, base, base+2, base+3});
}

void CloudManager::update(const glm::vec3 &cameraPos) {
  int currentChunkX = static_cast<int>(std::floor(cameraPos.x / m_width));
  int currentChunkZ = static_cast<int>(std::floor(cameraPos.z / m_depth));

  // Add chunks in range
  for (int dz = -renderDistance; dz <= renderDistance; ++dz) {
    for (int dx = -renderDistance; dx <= renderDistance; ++dx) {
      int cx = currentChunkX + dx;
      int cz = currentChunkZ + dz;
      long long key = makeKey(cx, cz);

      if (chunks.find(key) == chunks.end()) {
        CloudChunk chunk{cx, cz, Mesh()};
        generateChunk(cx, cz, chunk.mesh);
        chunks[key] = std::move(chunk);
      }
    }
  }

  // Optional: evict chunks outside renderDistance
  std::vector<long long> toRemove;
  for (auto &[key, chunk] : chunks) {
    int dx = chunk.cx - currentChunkX;
    int dz = chunk.cz - currentChunkZ;
    if (std::abs(dx) > renderDistance || std::abs(dz) > renderDistance) {
      toRemove.push_back(key);
    }
  }
  for (auto key : toRemove) {
    chunks.erase(key);
  }
}

void CloudManager::render(Shader &shader) const {
  shader.use();
  for (auto &[key, chunk] : chunks) {
    chunk.mesh.draw(shader);
  }
}
