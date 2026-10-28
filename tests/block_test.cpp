#include "doctest/doctest.h"
#include "block/block_data.hpp"
#include "item/item.hpp"
#include <algorithm>
#include <map>
#include <set>
#include <utility>

namespace {

constexpr uint16_t kLastBlock = static_cast<uint16_t>(kLastBlockType);

const std::vector<BlockType> kBuildingBlocks = {
    BlockType::OAK_PLANKS,   BlockType::PINE_PLANKS,  BlockType::PALM_PLANKS,
    BlockType::CHERRY_PLANKS, BlockType::BRICKS,      BlockType::STONE_BRICKS,
    BlockType::GLASS,        BlockType::WHITE_BLOCK,  BlockType::RED_BLOCK,
    BlockType::ORANGE_BLOCK, BlockType::YELLOW_BLOCK, BlockType::GREEN_BLOCK,
    BlockType::BLUE_BLOCK,   BlockType::PURPLE_BLOCK, BlockType::PINK_BLOCK,
};

} // namespace

TEST_CASE("block ids are stable: saves store them as numbers") {
  CHECK(static_cast<int>(BlockType::GLOWSTONE) == 25);
  CHECK(static_cast<int>(BlockType::CHERRY_LEAF) == 27);
  CHECK(static_cast<int>(BlockType::OAK_PLANKS) == 28);
  CHECK(static_cast<int>(BlockType::GLASS) == 34);
  CHECK(static_cast<int>(BlockType::PINK_BLOCK) == 42);
}

TEST_CASE("every block but air has an atlas entry") {
  for (uint16_t v = 1; v <= kLastBlock; ++v) {
    BlockType b = static_cast<BlockType>(v);
    CAPTURE(blockName(b));
    CHECK(block_data.count(b) == 1);
  }
}

TEST_CASE("no two blocks share a tile, apart from snow, dirt bottoms, furnace tops and doors") {
  std::map<std::pair<int, int>, std::set<BlockType>> users;
  for (const auto &[type, tex] : block_data)
    for (glm::ivec2 t : {tex.top, tex.bottom, tex.side})
      users[{t.x, t.y}].insert(type);
  // Deliberate shares: the snowy blocks share the snow top, and grass sits
  // on a dirt bottom.
  const std::set<BlockType> snow = {BlockType::SNOW, BlockType::SNOW_STONE,
                                    BlockType::SNOW_DIRT};
  const std::set<BlockType> dirt = {BlockType::GRASS, BlockType::DIRT};
  const std::set<BlockType> furnace = {BlockType::FURNACE, BlockType::FURNACE_LIT};
  // And a door's states all wear the same few tiles.
  auto doors = [](const std::set<BlockType> &types) {
    return std::all_of(types.begin(), types.end(), isDoor);
  };
  for (const auto &[tile, types] : users) {
    CAPTURE(tile.first);
    CAPTURE(tile.second);
    if (types.size() > 1)
      CHECK((types == snow || types == dirt || types == furnace || doors(types)));
  }
}

TEST_CASE("building blocks are named, solid, opaque cubes") {
  for (BlockType b : kBuildingBlocks) {
    CAPTURE(static_cast<int>(b));
    CHECK(blockName(b) != "Unknown");
    CHECK_FALSE(isTransparent(b));
    CHECK_FALSE(isLiquid(b));
    CHECK(blockLightEmission(b) == 0);
    if (b == BlockType::GLASS) continue;
    CHECK_FALSE(isCutout(b));
    CHECK(skyLightOpacity(b) == 15);
  }
}

TEST_CASE("glowstone is the lamp and still lights up") {
  CHECK(blockName(BlockType::GLOWSTONE) == "Lamp");
  CHECK(blockLightEmission(BlockType::GLOWSTONE) == 15);
}

TEST_CASE("building blocks are in the creative palette and drop themselves") {
  const auto &palette = paletteItems();
  for (BlockType b : kBuildingBlocks) {
    CAPTURE(blockName(b));
    ItemId id = itemForBlock(b);
    CHECK(std::find(palette.begin(), palette.end(), id) != palette.end());
    CHECK(dropFor(b) == id);
  }
}

TEST_CASE("glass is a see-through cutout that lets sky light through") {
  CHECK(isCutout(BlockType::GLASS));
  CHECK(skyLightOpacity(BlockType::GLASS) == 0);
}

TEST_CASE("glass beside glass hides the face between them; leaves don't") {
  CHECK_FALSE(shouldRenderFace(BlockType::GLASS, BlockType::GLASS));
  CHECK(shouldRenderFace(BlockType::OAK_LEAF, BlockType::OAK_LEAF));
}

TEST_CASE("the faces behind glass are still drawn") {
  CHECK(shouldRenderFace(BlockType::STONE, BlockType::GLASS));
  CHECK(shouldRenderFace(BlockType::GLASS, BlockType::AIR));
  CHECK(shouldRenderFace(BlockType::GLASS, BlockType::OAK_LEAF));
  CHECK_FALSE(shouldRenderFace(BlockType::GLASS, BlockType::STONE));
  CHECK_FALSE(shouldRenderFace(BlockType::BRICKS, BlockType::RED_BLOCK));
}

TEST_CASE("the glow crystal glows softer than the lamp and drops itself") {
  CHECK(static_cast<int>(BlockType::GLOW_CRYSTAL) == 43);
  CHECK(blockName(BlockType::GLOW_CRYSTAL) == "Glow Crystal");
  CHECK(blockLightEmission(BlockType::GLOW_CRYSTAL) == 10);
  CHECK(skyLightOpacity(BlockType::GLOW_CRYSTAL) == 15);
  CHECK(dropFor(BlockType::GLOW_CRYSTAL) == itemForBlock(BlockType::GLOW_CRYSTAL));
  const auto &palette = paletteItems();
  CHECK(std::find(palette.begin(), palette.end(),
                  itemForBlock(BlockType::GLOW_CRYSTAL)) != palette.end());
}

TEST_CASE("the crafting table is a plain solid block that drops itself") {
  CHECK(static_cast<int>(BlockType::CRAFTING_TABLE) == 44);
  CHECK(blockName(BlockType::CRAFTING_TABLE) == "Crafting Table");
  CHECK(blockLightEmission(BlockType::CRAFTING_TABLE) == 0);
  CHECK(skyLightOpacity(BlockType::CRAFTING_TABLE) == 15);
  CHECK_FALSE(isCutout(BlockType::CRAFTING_TABLE));
  CHECK(dropFor(BlockType::CRAFTING_TABLE) == itemForBlock(BlockType::CRAFTING_TABLE));
}

TEST_CASE("a lit furnace glows, drops a furnace and is never an item") {
  CHECK(static_cast<int>(BlockType::FURNACE) == 45);
  CHECK(static_cast<int>(BlockType::FURNACE_LIT) == 46);
  CHECK(blockLightEmission(BlockType::FURNACE_LIT) == 13);
  CHECK(blockLightEmission(BlockType::FURNACE) == 0);
  CHECK(dropFor(BlockType::FURNACE) == itemForBlock(BlockType::FURNACE));
  CHECK(dropFor(BlockType::FURNACE_LIT) == itemForBlock(BlockType::FURNACE));
  CHECK(isValidItem(itemForBlock(BlockType::FURNACE)));
  CHECK_FALSE(isValidItem(itemForBlock(BlockType::FURNACE_LIT)));
  const auto &palette = paletteItems();
  CHECK(std::find(palette.begin(), palette.end(), itemForBlock(BlockType::FURNACE_LIT)) ==
        palette.end());
}
