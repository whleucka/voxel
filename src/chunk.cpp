#include "chunk.hpp"
#include "block.hpp"
#include "block_type.hpp"
#include "world.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/noise.hpp>

// alias
using V3 = glm::vec3;
using V2 = glm::vec2;

// ------ Face vertex templates for a unit cube centered at the origin ------
// +X face (right)  CCW: bottom-front → bottom-back → top-back → top-front
static const V3 FACE_PX[4] = {V3(0.5f, -0.5f, 0.5f), V3(0.5f, -0.5f, -0.5f),
                              V3(0.5f, 0.5f, -0.5f), V3(0.5f, 0.5f, 0.5f)};

// -X face (left)   CCW: bottom-back → bottom-front → top-front → top-back
static const V3 FACE_NX[4] = {V3(-0.5f, -0.5f, -0.5f), V3(-0.5f, -0.5f, 0.5f),
                              V3(-0.5f, 0.5f, 0.5f), V3(-0.5f, 0.5f, -0.5f)};

// +Y face (top)    CCW: front-left → front-right → back-right → back-left
static const V3 FACE_PY[4] = {V3(-0.5f, 0.5f, 0.5f), V3(0.5f, 0.5f, 0.5f),
                              V3(0.5f, 0.5f, -0.5f), V3(-0.5f, 0.5f, -0.5f)};

// -Y face (bottom) CCW: back-left → back-right → front-right → front-left
static const V3 FACE_NY[4] = {V3(-0.5f, -0.5f, -0.5f), V3(0.5f, -0.5f, -0.5f),
                              V3(0.5f, -0.5f, 0.5f), V3(-0.5f, -0.5f, 0.5f)};

// +Z face (front)  CCW: bottom-left → bottom-right → top-right → top-left
static const V3 FACE_PZ[4] = {V3(-0.5f, -0.5f, 0.5f), V3(0.5f, -0.5f, 0.5f),
                              V3(0.5f, 0.5f, 0.5f), V3(-0.5f, 0.5f, 0.5f)};

// -Z face (back)   CCW: bottom-right → bottom-left → top-left → top-right
static const V3 FACE_NZ[4] = {V3(0.5f, -0.5f, -0.5f), V3(-0.5f, -0.5f, -0.5f),
                              V3(-0.5f, 0.5f, -0.5f), V3(0.5f, 0.5f, -0.5f)};

Chunk::Chunk(const int w, const int l, const int h, const int world_x,
             const int world_z, World *world)
    : width(w), length(l), height(h), world_x(world_x), world_z(world_z),
      world(world) {
  // Calculate AABB for the chunk
  m_aabb.min = glm::vec3(world_x * width, 0, world_z * length);
  m_aabb.max =
      glm::vec3(world_x * width + width, height, world_z * length + length);

  //~.~.~.~.~. TERRAIN GENERATION ~.~.~.~.~.~.~.~.~.~.~.~.~.~.~.~.~.~.
  blocks.resize(width,
                std::vector<std::vector<BlockType>>(
                    length, std::vector<BlockType>(height, BlockType::AIR)));

  const int h_mod = 29;       // Determines height of terrain
  const float h_freq = 0.02f; // perlin noise frequency
  for (int x = 0; x < width; x++) {
    for (int z = 0; z < length; z++) {
      // --- multi-octave Perlin noise (fBm) ---
      auto fbm = [](glm::vec2 p, int octaves, double lacunarity, double gain) {
        double sum = 0.0;
        double amplitude = 1.0;
        float frequency = 1.0;
        double norm = 0.0;

        for (int i = 0; i < octaves; i++) {
          sum += amplitude * (glm::perlin(p * frequency) * 0.5 + 0.5);
          norm += amplitude;
          amplitude *= gain;
          frequency *= lacunarity;
        }
        return sum / norm; // normalize back to [0,1]
      };

      // Sample noise at world position
      glm::vec2 pos((world_x * width + x) * h_freq,
                    (world_z * length + z) * h_freq);

      // 4 octaves, lacunarity=2.0, gain=0.5 → classic smooth fBm
      double hNoise = fbm(pos, 4, 1.8, 0.5);
      // // Scale to terrain height
      // int perlin_height = static_cast<int>(hNoise * 30) + h_mod;

      // --- large scale biome noise ---
      glm::vec2 biome_pos((world_x * width + x) * 0.001f, // much lower frequency
                          (world_z * length + z) * 0.001f);

      double biome_noise = glm::perlin(biome_pos) * 0.5 + 0.5; // [0,1]

      // Emphasize mountain regions (smoothstep makes it "rare")
      // I'll probably forget what this means:
      // Takes your biome noise, says “only the highest values should count as mountains,” 
      // and applies a smooth ramp so that instead of an ugly cliff at 0.45, 
      // you get a nice smooth transition into mountains.
      double mountain_factor = glm::smoothstep(0.45, 0.85, biome_noise);

      // Final height = base hills + mountains
      int perlin_height =
          static_cast<int>(
              hNoise * 30            // base hills
              + mountain_factor * 80 // extra mountains only where biome is high
              ) +
          h_mod;

      // Set block type
      for (int y = 0; y < perlin_height; y++) {
        BlockType type = BlockType::AIR;
        const double stone_noise =
            glm::perlin(V3((world_x * width + x) * 0.55, y * 0.25,
                           (world_z * length + z) * 0.25));
        const double bedrock_noise =
            glm::perlin(V3((world_x * width + x) * 0.42, y * 0.02,
                           (world_z * length + z) * 0.42));
        // const double air_noise =
        //     glm::perlin(V3((world_x * width + x) * 0.02, y * 0.02,
        //                    (world_z * length + z) * 0.02));

        if (y == perlin_height - 1) {
          int block_rand = (rand() % 100) + 1;
          if (perlin_height >= world->snow_height) {
            type = BlockType::SNOW;
          } else if (perlin_height <= world->sea_level) {
            type = BlockType::SAND;
          } else {
            if (perlin_height <= world->sea_level + 5) {
              type = BlockType::GRASS;
            } else {
              if (block_rand <= 5) {
                type = BlockType::COBBLESTONE;
              } else if (block_rand <= 85) {
                type = BlockType::STONE;
              } else {
                type = BlockType::DIRT;
              }
            }
          }
        } else if ((bedrock_noise > 0.1 && y <= 6) || (y < 5)) {
          type = BlockType::BEDROCK;
        } else if ((stone_noise > 0.4) || (y >= 5 && y <= 15)) {
          type = BlockType::STONE;
        // } else if (air_noise > 0.4 && y <= 40) {
        //   // Cave-like
        //   type = BlockType::AIR;
        } else {
          type = BlockType::DIRT;
        }
        

        blocks[x][z][y] = type;
      }

      // Fill water
      for (int y = perlin_height; y < world->sea_level; y++) {
        if (blocks[x][z][y] == BlockType::AIR) {
          blocks[x][z][y] = BlockType::WATER;
        }
      }
    }
  }
}

Chunk::~Chunk() {}

void Chunk::generateMesh(const Texture &atlas) {
  opaqueMesh.vertices.clear();
  opaqueMesh.indices.clear();
  opaqueMesh.textures.clear();
  opaqueMesh.textures.push_back(atlas);

  transparentMesh.vertices.clear();
  transparentMesh.indices.clear();
  transparentMesh.textures.clear();
  transparentMesh.textures.push_back(atlas);

  for (int x = 0; x < width; x++) {
    for (int z = 0; z < length; z++) {
      for (int y = 0; y < height; y++) {
        if (blocks[x][z][y] == BlockType::AIR) continue;

        const BlockType type = blocks[x][z][y];
        const auto map = Block::tilesFor(type);
        Block block(type, V3(x, y, z));

        // Choose target mesh
        Mesh &target = isTransparent(type) ? transparentMesh : opaqueMesh;

        if (faceVisible(x, y, z, 0, type)) block.emitFace(FACE_PX, V3(1, 0, 0), map.px, target);
        if (faceVisible(x, y, z, 1, type)) block.emitFace(FACE_NX, V3(-1, 0, 0), map.nx, target);
        if (faceVisible(x, y, z, 2, type)) block.emitFace(FACE_PY, V3(0, 1, 0), map.py, target);
        if (faceVisible(x, y, z, 3, type)) block.emitFace(FACE_NY, V3(0, -1, 0), map.ny, target);
        if (faceVisible(x, y, z, 4, type)) block.emitFace(FACE_PZ, V3(0, 0, 1), map.pz, target);
        if (faceVisible(x, y, z, 5, type)) block.emitFace(FACE_NZ, V3(0, 0, -1), map.nz, target);
      }
    }
  }
}

void Chunk::drawOpaque(Shader &shader) {
  opaqueMesh.draw(shader);
}

void Chunk::drawTransparent(Shader &shader) {
  transparentMesh.draw(shader);
}

BlockType Chunk::getBlock(int x, int y, int z) const {
  if (x < 0 || x >= width || y < 0 || y >= height || z < 0 || z >= length) {
    return BlockType::AIR;
  }
  return blocks[x][z][y];
}

bool Chunk::faceVisible(int x, int y, int z, int dir, BlockType currentBlockType) const {
    int nx = x, ny = y, nz = z;
    switch (dir) {
        case 0: nx++; break;
        case 1: nx--; break;
        case 2: ny++; break;
        case 3: ny--; break;
        case 4: nz++; break;
        case 5: nz--; break;
    }

    auto getNeighbor = [&](int nx, int ny, int nz) -> BlockType {
        if (nx >= 0 && nx < width && ny >= 0 && ny < height && nz >= 0 && nz < length)
            return blocks[nx][nz][ny];
        int gx = world_x * width + nx;
        int gy = ny;
        int gz = world_z * length + nz;
        return world->getBlock(gx, gy, gz);
    };

    BlockType neighborType = getNeighbor(nx, ny, nz);

    // ✅ NEW: Don’t emit seam faces if neighbor isn’t loaded
    if (neighborType == BlockType::UNKNOWN) {
        return false;
    }

    if (isTransparent(currentBlockType)) {
        // Water: only draw surfaces against air
        return neighborType == BlockType::AIR;
    } else {
        // Solids: draw against air OR transparent (so sand shows under water)
        return (neighborType == BlockType::AIR || isTransparent(neighborType));
    }
}

ChunkKey Chunk::getChunkKey() const { return makeChunkKey(world_x, world_z); }

void Chunk::setBlock(int x, int y, int z, BlockType type) {
  if (x < 0 || x >= width || y < 0 || y >= height || z < 0 || z >= length) {
    return;
  }
  blocks[x][z][y] = type;
}

