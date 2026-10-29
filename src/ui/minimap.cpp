#include "ui/minimap.hpp"
#include "block/bed.hpp"
#include "block/block_data.hpp"
#include "block/crop.hpp"
#include "block/door.hpp"
#include "chunk/chunk.hpp"
#include "core/constants.hpp"
#include "world/world.hpp"
#include <cmath>
#include <memory>

namespace {

// Sky light at the head this low or lower: under rock.
constexpr uint8_t kCaveSkyLight = 6;

constexpr glm::u8vec4 kNothing{0, 0, 0, 0};
constexpr glm::u8vec4 kCaveOpen{214, 200, 168, 255};
constexpr glm::u8vec4 kCaveRock{46, 42, 52, 255};
constexpr glm::u8vec4 kCaveWater{58, 104, 196, 255};

constexpr glm::u8vec4 rgb(uint8_t r, uint8_t g, uint8_t b) { return {r, g, b, 255}; }

glm::u8vec4 shade(glm::u8vec4 c, float f) {
  auto ch = [f](uint8_t v) {
    return static_cast<uint8_t>(std::clamp(std::lround(v * f), 0l, 255l));
  };
  return {ch(c.r), ch(c.g), ch(c.b), c.a};
}

int floorDiv(int a, int b) { return a >= 0 ? a / b : -((-a + b - 1) / b); }

// Reads columns of the loaded world, keeping hold of the last chunk.
struct Columns {
  const World &world;
  std::shared_ptr<const Chunk> chunk;
  int cx = 0, cz = 0;
  bool have = false;

  // The chunk holding column (x, z), and the column inside it.
  const Chunk *at(int x, int z, int &lx, int &lz) {
    int want_x = floorDiv(x, kChunkWidth), want_z = floorDiv(z, kChunkDepth);
    if (!have || want_x != cx || want_z != cz) {
      chunk = world.getChunk(want_x, want_z);
      cx = want_x, cz = want_z, have = true;
    }
    lx = x - cx * kChunkWidth;
    lz = z - cz * kChunkDepth;
    return chunk.get();
  }
};

} // namespace

glm::u8vec4 mapColour(BlockType type) {
  using B = BlockType;
  if (isBed(type)) {
    static constexpr glm::u8vec4 kBeds[kBedColours] = {
        rgb(236, 236, 236), rgb(196, 52, 48),  rgb(232, 132, 40), rgb(236, 208, 56),
        rgb(84, 172, 64),   rgb(56, 96, 200),  rgb(132, 72, 184), rgb(236, 140, 180)};
    return kBeds[static_cast<int>(bedColour(type))];
  }
  if (isDoor(type)) return rgb(150, 110, 64);
  if (isCrop(type)) return rgb(150, 176, 64);
  if (isTorch(type)) return rgb(250, 214, 100);
  if (isCake(type)) return rgb(244, 230, 220);
  if (isFarmland(type)) return rgb(110, 76, 46);
  switch (type) {
    case B::AIR: return kNothing;
    case B::GRASS: return rgb(96, 160, 60);
    case B::DIRT: return rgb(134, 96, 62);
    case B::SAND: return rgb(222, 206, 150);
    case B::SANDSTONE: return rgb(206, 186, 128);
    case B::SNOW: case B::SNOW_DIRT: return rgb(240, 244, 250);
    case B::SNOW_STONE: return rgb(200, 206, 214);
    case B::WATER: return rgb(58, 104, 196);
    case B::OAK_LEAF: return rgb(56, 120, 40);
    case B::PINE_LEAF: return rgb(40, 92, 52);
    case B::PALM_LEAF: return rgb(76, 150, 44);
    case B::CHERRY_LEAF: return rgb(236, 162, 196);
    case B::OAK_LOG: case B::PINE_LOG: case B::PALM_LOG: case B::CHERRY_LOG:
      return rgb(110, 82, 52);
    case B::OAK_PLANKS: case B::PINE_PLANKS: case B::PALM_PLANKS: case B::CHERRY_PLANKS:
    case B::CRAFTING_TABLE: case B::CHEST:
      return rgb(176, 136, 84);
    case B::BRICKS: return rgb(160, 78, 62);
    case B::GLASS: return rgb(190, 220, 230);
    case B::GLOWSTONE: case B::GLOW_CRYSTAL: case B::FURNACE_LIT: return rgb(250, 214, 100);
    case B::WHITE_BLOCK: return rgb(236, 236, 236);
    case B::RED_BLOCK: return rgb(196, 52, 48);
    case B::ORANGE_BLOCK: return rgb(232, 132, 40);
    case B::YELLOW_BLOCK: return rgb(236, 208, 56);
    case B::GREEN_BLOCK: return rgb(84, 172, 64);
    case B::BLUE_BLOCK: return rgb(56, 96, 200);
    case B::PURPLE_BLOCK: return rgb(132, 72, 184);
    case B::PINK_BLOCK: return rgb(236, 140, 180);
    case B::BEDROCK: return rgb(60, 60, 64);
    case B::TALL_GRASS: case B::FERN: return rgb(96, 160, 60); // like the grass under it
    case B::POPPY: return rgb(214, 46, 46);
    case B::BUTTERCUP: return rgb(246, 214, 46);
    case B::CORNFLOWER: return rgb(72, 110, 220);
    case B::PINK_TULIP: case B::PINK_CORAL: return rgb(240, 140, 186);
    case B::DAISY: case B::SHELLS: return rgb(240, 238, 228);
    case B::LAVENDER: return rgb(150, 110, 210);
    case B::HIBISCUS: return rgb(240, 120, 40);
    case B::RED_MUSHROOM: return rgb(200, 40, 40);
    case B::BROWN_MUSHROOM: return rgb(150, 110, 80);
    case B::DEAD_BUSH: return rgb(150, 112, 70);
    case B::BERRY_BUSH: case B::BERRY_BUSH_BARE: return rgb(56, 120, 50);
    case B::CACTUS: return rgb(70, 140, 60);
    case B::BLUE_CORAL: return rgb(60, 120, 230);
    case B::YELLOW_CORAL: return rgb(240, 210, 60);
    case B::SANDSTONE_BRICKS: return rgb(206, 186, 128);
    case B::SNOW_BRICKS: return rgb(230, 236, 244);
    default: return rgb(128, 128, 132); // stone, ores, cobble and the rest
  }
}

bool underRock(const World &world, glm::vec3 feet) {
  const glm::ivec3 head(glm::floor(feet + glm::vec3(0.0f, kPlayerEyeHeight, 0.0f)));
  // Not loaded yet: nothing is known, least of all that it's a cave.
  if (!world.getChunk(floorDiv(head.x, kChunkWidth), floorDiv(head.z, kChunkDepth))) return false;
  if (world.getBlockAt(glm::vec3(head) + 0.5f) == BlockType::WATER) return false;
  return world.getSkyLightAt(head) <= kCaveSkyLight;
}

glm::vec2 mapOffset(glm::vec2 delta, float yaw) {
  const glm::vec2 front(std::cos(glm::radians(yaw)), std::sin(glm::radians(yaw)));
  const glm::vec2 right(-front.y, front.x);
  return {glm::dot(delta, right), -glm::dot(delta, front)};
}

MarkerSpot placeMarker(glm::vec2 offset, float radius) {
  const float len = glm::length(offset);
  if (len <= radius) return {offset, false};
  return {offset / len * radius, true};
}

void Minimap::update(const World &world, glm::vec3 feet, float dt) {
  const View now = underRock(world, feet) ? View::Cave : View::Surface;
  if (now != pending_) pending_ = now, pending_s_ = 0.0f;
  pending_s_ += dt;
  const bool switched = pending_ != view_ && pending_s_ >= kSwitchSeconds;
  if (switched) view_ = pending_;

  since_sample_ += dt;
  const glm::ivec3 here(glm::floor(feet));
  if (!sampled_ || switched || here != centre_ || since_sample_ >= kRefreshSeconds)
    sample(world, here, view_);
}

void Minimap::sample(const World &world, glm::ivec3 centre, View view) {
  centre_ = centre;
  view_ = view;
  sampled_ = true;
  since_sample_ = 0.0f;
  Columns cols{world, nullptr};
  std::array<int, kSize * kSize> heights{};

  for (int j = 0; j < kSize; ++j)
    for (int i = 0; i < kSize; ++i) {
      glm::u8vec4 &out = cells_[j * kSize + i];
      int lx, lz;
      const Chunk *c = cols.at(centre.x - kRadius + i, centre.z - kRadius + j, lx, lz);
      if (!c) {
        out = kNothing;
        continue;
      }
      if (view == View::Surface) {
        int y = kChunkHeight - 1;
        while (y > 0 && c->safeAt(lx, y, lz) == BlockType::AIR) --y;
        heights[j * kSize + i] = y;
        out = mapColour(c->safeAt(lx, y, lz));
        continue;
      }
      // The slice: room to stand at the feet, or a little above or below.
      out = kCaveRock;
      for (int dy : {0, 1, -1, 2, -2}) {
        const int y = centre.y + dy;
        const BlockType low = c->safeAt(lx, y, lz);
        if (blocksMovement(low) || blocksMovement(c->safeAt(lx, y + 1, lz))) continue;
        out = low == BlockType::WATER ? kCaveWater : shade(kCaveOpen, 1.0f - 0.1f * std::abs(dy));
        break;
      }
    }

  // Light from the north-west: a column higher than its neighbour there is
  // lit, a lower one shaded.
  if (view == View::Surface)
    for (int j = 1; j < kSize; ++j)
      for (int i = 1; i < kSize; ++i) {
        glm::u8vec4 &out = cells_[j * kSize + i];
        if (out.a == 0 || cells_[(j - 1) * kSize + i - 1].a == 0) continue;
        const int rise = heights[j * kSize + i] - heights[(j - 1) * kSize + i - 1];
        if (rise > 0) out = shade(out, 1.15f);
        else if (rise < 0) out = shade(out, 0.82f);
      }
}

glm::u8vec4 Minimap::cell(int dx, int dz) const {
  if (std::abs(dx) > kRadius || std::abs(dz) > kRadius) return kNothing;
  return cells_[(dz + kRadius) * kSize + dx + kRadius];
}
