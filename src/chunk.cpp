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
static const V3 FACE_PX[4] = {
    V3(0.5f, -0.5f,  0.5f),
    V3(0.5f, -0.5f, -0.5f),
    V3(0.5f,  0.5f, -0.5f),
    V3(0.5f,  0.5f,  0.5f)
};

// -X face (left)   CCW: bottom-back → bottom-front → top-front → top-back
static const V3 FACE_NX[4] = {
    V3(-0.5f, -0.5f, -0.5f),
    V3(-0.5f, -0.5f,  0.5f),
    V3(-0.5f,  0.5f,  0.5f),
    V3(-0.5f,  0.5f, -0.5f)
};

// +Y face (top)    CCW: front-left → front-right → back-right → back-left
static const V3 FACE_PY[4] = {
    V3(-0.5f,  0.5f,  0.5f),
    V3( 0.5f,  0.5f,  0.5f),
    V3( 0.5f,  0.5f, -0.5f),
    V3(-0.5f,  0.5f, -0.5f)
};

// -Y face (bottom) CCW: back-left → back-right → front-right → front-left
static const V3 FACE_NY[4] = {
    V3(-0.5f, -0.5f, -0.5f),
    V3( 0.5f, -0.5f, -0.5f),
    V3( 0.5f, -0.5f,  0.5f),
    V3(-0.5f, -0.5f,  0.5f)
};

// +Z face (front)  CCW: bottom-left → bottom-right → top-right → top-left
static const V3 FACE_PZ[4] = {
    V3(-0.5f, -0.5f,  0.5f),
    V3( 0.5f, -0.5f,  0.5f),
    V3( 0.5f,  0.5f,  0.5f),
    V3(-0.5f,  0.5f,  0.5f)
};

// -Z face (back)   CCW: bottom-right → bottom-left → top-left → top-right
static const V3 FACE_NZ[4] = {
    V3( 0.5f, -0.5f, -0.5f),
    V3(-0.5f, -0.5f, -0.5f),
    V3(-0.5f,  0.5f, -0.5f),
    V3( 0.5f,  0.5f, -0.5f)
};

Chunk::Chunk(const int w, const int l, const int h, const int world_x,
             const int world_z, World *world)
    : width(w), length(l), height(h), world_x(world_x), world_z(world_z), world(world) {
  blocks.resize(width,
                std::vector<std::vector<BlockType>>(
                    length, std::vector<BlockType>(height, BlockType::AIR)));

  const int SEA_LEVEL = 50;

  for (int x = 0; x < width; x++) {
    for (int z = 0; z < length; z++) {
      V2 pos(
          (world_x * width + x) * 0.03f,
          (world_z * length + z) * 0.03f);
      double hNoise = glm::perlin(pos) * 0.5 + 0.5; // [0,1]
      int perlin_height = static_cast<int>(hNoise * 30) + 40;

      // --- terrain generation ---
      for (int y = 0; y < perlin_height; y++) {
        BlockType type = BlockType::AIR;
        const double stone_noise =
          glm::perlin(V3((world_x * width + x) * 0.55, y * 0.25,
                (world_z * length + z) * 0.25));
        const double bedrock_noise =
          glm::perlin(V3((world_x * width + x) * 0.42, y * 0.02,
                (world_z * length + z) * 0.42));
        const double water_noise =
          glm::perlin(V3((world_x * width + x) * 0.02, y * 0.02,
                (world_z * length + z) * 0.02));

        if (y == perlin_height - 1) {
          type = BlockType::GRASS;
        } else if ((bedrock_noise > 0.1 && y <= 10) || (y < 5)) {
          type = BlockType::BEDROCK;
        } else if ((stone_noise > 0.4) || (y >= 5 && y <= 15)) {
          type = BlockType::STONE;
        } else if (water_noise > 0.4 && y <= 40) {
          type = BlockType::WATER;
        } else {
          type = BlockType::DIRT;
        }
        blocks[x][z][y] = type;
      }

      // --- water filling pass ---
      for (int y = perlin_height; y < SEA_LEVEL; y++) {
        if (blocks[x][z][y] == BlockType::AIR) {
          blocks[x][z][y] = BlockType::WATER;
        }
      }
    }
  }
}

void Chunk::generateMesh(const Texture &atlas) {
  mesh.textures.push_back(atlas);
  for (int x = 0; x < width; x++) {
    for (int z = 0; z < length; z++) {
      for (int y = 0; y < height; y++) {
        if (blocks[x][z][y] == BlockType::AIR) {
          continue;
        }

        const BlockType type = blocks[x][z][y];
        const auto map = Block::tilesFor(type);
        Block block(type, V3(x, y, z));

        if (faceVisible(x, y, z, 0)) { // +X
          block.emitFace(FACE_PX, V3(1, 0, 0), map.px, mesh);
        }
        if (faceVisible(x, y, z, 1)) { // -X
          block.emitFace(FACE_NX, V3(-1, 0, 0), map.nx, mesh);
        }
        if (faceVisible(x, y, z, 2)) { // +Y
          block.emitFace(FACE_PY, V3(0, 1, 0), map.py, mesh);
        }
        if (faceVisible(x, y, z, 3)) { // -Y
          block.emitFace(FACE_NY, V3(0, -1, 0), map.ny, mesh);
        }
        if (faceVisible(x, y, z, 4)) { // +Z
          block.emitFace(FACE_PZ, V3(0, 0, 1), map.pz, mesh);
        }
        if (faceVisible(x, y, z, 5)) { // -Z
          block.emitFace(FACE_NZ, V3(0, 0, -1), map.nz, mesh);
        }
      }
    }
  }
  mesh.setupMesh();
}

Chunk::~Chunk() {}

void Chunk::draw(Shader &shader) { mesh.draw(shader); }

BlockType Chunk::getBlock(int x, int y, int z) const {
  if (x < 0 || x >= width || y < 0 || y >= height || z < 0 || z >= length) {
    return BlockType::AIR;
  }
  return blocks[x][z][y];
}

bool Chunk::faceVisible(int x, int y, int z, int dir) const {
  int nx = x, ny = y, nz = z;
  switch (dir) {
    case 0: nx++; break; // +X
    case 1: nx--; break; // -X
    case 2: ny++; break; // +Y
    case 3: ny--; break; // -Y
    case 4: nz++; break; // +Z
    case 5: nz--; break; // -Z
  }

  // 1) If neighbor is inside this chunk, use local data (fast + robust)
  if (nx >= 0 && nx < width &&
      ny >= 0 && ny < height &&
      nz >= 0 && nz < length) {
    return blocks[nx][nz][ny] == BlockType::AIR;
  }

  // 2) Otherwise, query the world using GLOBAL coords derived from chunk indices.
  //    NOTE: in your ctor, world_x/world_z are *chunk indices*, not block offsets.
  const int gx = world_x * width  + nx;
  const int gy = ny;
  const int gz = world_z * length + nz;

  return world->getBlock(gx, gy, gz) == BlockType::AIR;
}
