#pragma once

#include "block/block_type.hpp"
#include "block/bed.hpp"
#include "block/crop.hpp"
#include "block/door.hpp"
#include "block/fence.hpp"
#include "block/plant.hpp"
#include "block/stairs.hpp"
#include "robin_hood/robin_hood.h"
#include <glm/glm.hpp>
#include <string_view>

struct BlockTexture {
  glm::ivec2 top;
  glm::ivec2 bottom;
  glm::ivec2 side;
};

inline bool isTransparent(BlockType type) {
  return type == BlockType::WATER;
}

inline bool isFullyTransparent(BlockType type) {
  return type == BlockType::AIR;
}

// Liquid blocks only render top surface, not sides
inline bool isLiquid(BlockType type) {
  return type == BlockType::WATER;
}

// Whether a block stops bodies and creatures: anything but air, water, an
// open door or gate, a crop, a plant, a torch and a ladder. A closed door
// fills its whole cell; a bed or a cake only its lower part (blockHeight).
inline bool blocksMovement(BlockType type) {
  return type != BlockType::AIR && !isLiquid(type) && !(isDoor(type) && doorOpen(type)) &&
         !isCrop(type) && !isPlant(type) && !isTorch(type) && !(isGate(type) && gateOpen(type)) &&
         !isLadder(type);
}

// Open to walk through, like air: bodies, paths and spawns pass these.
inline bool isPassable(BlockType type) { return !blocksMovement(type) && !isLiquid(type); }

// How high up its cell a block that blocksMovement stops bodies: the top a
// foot lands on (a stair's back step is higher still).
inline float blockHeight(BlockType type) {
  return isFence(type) || isGate(type) ? kFenceHeight
         : isBed(type)  ? kBedHeight
         : isCake(type) ? kCakeHeight
         : isSlab(type) || isStair(type) ? kSlabHeight
                                         : 1.0f;
}

// The solid parts of a cell, in cell space (0..1): what bodies bump into,
// and what the mesh draws for a slab or a stair. None for what's walked
// through; a closed door fills its whole cell. A fence's and a gate's reach
// above the cell (kFenceHeight), so nothing jumps them.
struct CellBoxes {
  DoorBox box[5];
  int count = 0;
  const DoorBox *begin() const { return box; }
  const DoorBox *end() const { return box + count; }
  void add(DoorBox b) { box[count++] = b; }
};

constexpr float kFencePostLo = 6.0f / 16.0f, kFencePostHi = 10.0f / 16.0f;
constexpr float kFenceRailLo = 7.0f / 16.0f, kFenceRailHi = 9.0f / 16.0f;

// A fence's post alone (blockBoxes) or with its rails (fenceBoxes); a closed
// gate's bar across its cell.
inline CellBoxes blockBoxes(BlockType type) {
  if (!blocksMovement(type)) return {};
  if (isFence(type))
    return {{{{kFencePostLo, 0, kFencePostLo}, {kFencePostHi, kFenceHeight, kFencePostHi}}}, 1};
  if (isGate(type))
    return {{gateAlongX(type)
                 ? DoorBox{{0, 0, kFenceRailLo}, {1, kFenceHeight, kFenceRailHi}}
                 : DoorBox{{kFenceRailLo, 0, 0}, {kFenceRailHi, kFenceHeight, 1}}},
            1};
  CellBoxes boxes{{{{0, 0, 0}, {1, blockHeight(type), 1}}}, 1};
  if (isStair(type)) boxes.add(stairStep(stairFacing(type)));
  return boxes;
}

// Whether `local` (in cell space) is inside a solid part of the cell.
inline bool insideBlock(BlockType type, glm::vec3 local) {
  for (const DoorBox &b : blockBoxes(type))
    if (glm::all(glm::greaterThanEqual(local, b.lo)) && glm::all(glm::lessThan(local, b.hi)))
      return true;
  return false;
}

// A full, solid block that a torch, a mushroom or a cake can sit on.
inline bool isSolidGround(BlockType type) {
  return blocksMovement(type) && blockHeight(type) == 1.0f && !isDoor(type) &&
         !needsSupport(type) && !isHalfShape(type);
}

// Whether a fence reaches over to `neighbour`, a unit step `toward` away
// (y 0): to another fence, a gate in line with it, or a full solid block.
inline bool fenceJoins(BlockType neighbour, glm::ivec3 toward) {
  if (isFence(neighbour)) return true;
  if (isGate(neighbour)) return gateAlongX(neighbour) == (toward.x != 0);
  return isSolidGround(neighbour);
}

// The four sides, N E S W, as unit steps.
constexpr glm::ivec3 kSides[4] = {{0, 0, -1}, {1, 0, 0}, {0, 0, 1}, {-1, 0, 0}};

// A fence's post and a rail out to each side it joins; `around` holds the
// blocks beside it, N E S W.
inline CellBoxes fenceBoxes(BlockType fence, const BlockType (&around)[4]) {
  CellBoxes boxes = blockBoxes(fence);
  constexpr float lo = kFenceRailLo, hi = kFenceRailHi, h = kFenceHeight;
  constexpr DoorBox rails[4] = {{{lo, 0, 0}, {hi, h, kFencePostLo}},
                                {{kFencePostHi, 0, lo}, {1, h, hi}},
                                {{lo, 0, kFencePostHi}, {hi, h, 1}},
                                {{0, 0, lo}, {kFencePostLo, h, hi}}};
  for (int i = 0; i < 4; ++i)
    if (fenceJoins(around[i], kSides[i])) boxes.add(rails[i]);
  return boxes;
}

// Drawn by a shape of their own in the chunk mesh, not as a cube.
inline bool hasOwnShape(BlockType type) {
  return isDoor(type) || isBed(type) || isCrop(type) || isPlant(type) || isTorch(type) ||
         isCake(type) || type == BlockType::CACTUS || isHalfShape(type) || isFence(type) ||
         isGate(type) || isLadder(type);
}

// Cutout blocks discard the faint texels of their tile instead of drawing them,
// which turns the soft edges of the leaf art into see-through holes.  They stay
// in the opaque pass (depth writes on, no sorting needed) but must not cull the
// faces behind them, since you can see straight through the holes.
//
// The threshold cannot be shared: the four leaf tiles were drawn with very
// different alpha ranges, so one value would leave pine solid (its alpha floor
// is 159) while dissolving oak.  These classes each open up ~25% of their tile.
// The cut is never global — the water tile is a uniform alpha 153, so any
// threshold that opens the leaves would erase every water surface in the world.
//
// Encoded as a 2-bit class in BlockVertex::ao bits[7:6].  The matching alpha
// values live in the CUTOUT_THRESHOLD table in block.vert / shadow_depth.vert
// and must stay in the same order.
enum CutoutClass : uint8_t {
  kCutoutNone = 0,  // no alpha test
  kCutoutLow  = 1,  // 0.67 — oak, glass, glow crystal
  kCutoutMid  = 2,  // 0.80 — palm, cherry
  kCutoutHigh = 3,  // 0.92 — pine
};

inline uint8_t cutoutClass(BlockType type) {
  switch (type) {
    case BlockType::OAK_LEAF:
    case BlockType::GLASS:       // frame 255, pane 0
    case BlockType::GLOW_CRYSTAL: return kCutoutLow; // shards 255, the rest 0
    case BlockType::PALM_LEAF:
    case BlockType::CHERRY_LEAF: return kCutoutMid;
    case BlockType::PINE_LEAF:   return kCutoutHigh;
    default:
      // A door's top half has a window in it, and a crop, a plant or a
      // torch is leaves (or a stick) on air.
      return (isDoor(type) && doorTop(type)) || isCrop(type) || isPlant(type) || isTorch(type) ||
                     isLadder(type)
                 ? kCutoutLow
                 : kCutoutNone;
  }
}

inline bool isCutout(BlockType type) {
  return cutoutClass(type) != kCutoutNone;
}

// Determine if a face should be rendered between current block and neighbor
inline bool shouldRenderFace(BlockType current, BlockType neighbor) {
  if (neighbor == BlockType::AIR) return true;
  // A door is a thin panel, and a bed, a cake, a cactus, a slab and a stair
  // don't fill their cell: whatever is round them shows.
  if (isDoor(neighbor) || isBed(neighbor) || isCake(neighbor) || neighbor == BlockType::CACTUS ||
      isHalfShape(neighbor) || isFence(neighbor) || isGate(neighbor))
    return true;
  // Panes of glass side by side read as one window, so the face between them
  // is hidden.  Leaves keep their inner faces (below).
  if (current == BlockType::GLASS && neighbor == BlockType::GLASS) return false;
  // Cutout neighbours (leaves) have see-through holes, so the face behind them
  // is visible and must be emitted.  This includes leaf-against-leaf, which is
  // what gives the canopy interior depth instead of looking hollow through the
  // holes.
  if (isCutout(neighbor)) return true;
  if (isTransparent(current)) {
    // Transparent blocks: only render if neighbor is different type
    return neighbor != current;
  }
  // Opaque blocks: render if neighbor is transparent (visible through it)
  return isTransparent(neighbor);
}

// Returns the number of sky-light levels absorbed when light passes through
// a block.  0 = fully transparent (air), 15 = fully opaque (stone, dirt…).
inline uint8_t skyLightOpacity(BlockType type) {
  switch (type) {
    case BlockType::AIR:
    case BlockType::GLASS:
    case BlockType::GLOW_CRYSTAL: return 0;
    case BlockType::WATER:     return 2;
    case BlockType::OAK_LEAF:
    case BlockType::PINE_LEAF:
    case BlockType::PALM_LEAF:
    case BlockType::CHERRY_LEAF: return 1;
    default: // light gets round these
      // (and round a cactus, a slab or a stair, which don't fill their
      // cells either)
      return isDoor(type) || isBed(type) || isCrop(type) || isPlant(type) || isTorch(type) ||
                     isCake(type) || type == BlockType::CACTUS || isHalfShape(type) ||
                     isFence(type) || isGate(type) || isLadder(type)
                 ? 0
                 : 15;
  }
}

// Light a block emits on its own, 0 (none) .. 15 (max, like glowstone).
// Emissive blocks seed the block-light BFS in Chunk::computeBlockLight().
inline uint8_t blockLightEmission(BlockType type) {
  switch (type) {
    case BlockType::GLOWSTONE:    return 15;
    case BlockType::GLOW_CRYSTAL: return 10; // softer than the lamp
    case BlockType::FURNACE_LIT:  return 13;
    default:                   return isTorch(type) ? 13 : 0;
  }
}

constexpr std::string_view kBedNames[kBedColours] = {
    "White Bed", "Red Bed", "Orange Bed", "Yellow Bed",
    "Green Bed", "Blue Bed", "Purple Bed", "Pink Bed"};

inline std::string_view blockName(BlockType type) {
  switch (type) {
    case BlockType::AIR:          return "Air";
    case BlockType::BEDROCK:      return "Bedrock";
    case BlockType::COBBLESTONE:  return "Cobblestone";
    case BlockType::DIRT:         return "Dirt";
    case BlockType::GRASS:        return "Grass";
    case BlockType::SAND:         return "Sand";
    case BlockType::SANDSTONE:    return "Sandstone";
    case BlockType::SNOW:         return "Snow";
    case BlockType::SNOW_DIRT:    return "Snow Dirt";
    case BlockType::SNOW_STONE:   return "Snow Stone";
    case BlockType::STONE:        return "Stone";
    case BlockType::OAK_LOG:      return "Oak Log";
    case BlockType::OAK_LEAF:     return "Oak Leaf";
    case BlockType::PINE_LOG:     return "Pine Log";
    case BlockType::PINE_LEAF:    return "Pine Leaf";
    case BlockType::PALM_LOG:     return "Palm Log";
    case BlockType::PALM_LEAF:    return "Palm Leaf";
    case BlockType::CHERRY_LOG:   return "Cherry Log";
    case BlockType::CHERRY_LEAF:  return "Cherry Blossom";
    case BlockType::WATER:        return "Water";
    case BlockType::COAL_ORE:     return "Coal Ore";
    case BlockType::GOLD_ORE:     return "Gold Ore";
    case BlockType::DIAMOND_ORE:  return "Diamond Ore";
    case BlockType::IRON_ORE:     return "Iron Ore";
    case BlockType::EMERALD_ORE:  return "Emerald Ore";
    case BlockType::RUBY_ORE:     return "Ruby Ore";
    case BlockType::COPPER_ORE:   return "Copper Ore";
    case BlockType::GLOWSTONE:    return "Lamp";  // enum name kept for saves
    case BlockType::OAK_PLANKS:    return "Oak Planks";
    case BlockType::PINE_PLANKS:   return "Pine Planks";
    case BlockType::PALM_PLANKS:   return "Palm Planks";
    case BlockType::CHERRY_PLANKS: return "Cherry Planks";
    case BlockType::BRICKS:        return "Bricks";
    case BlockType::STONE_BRICKS:  return "Stone Bricks";
    case BlockType::GLASS:         return "Glass";
    case BlockType::WHITE_BLOCK:   return "White Block";
    case BlockType::RED_BLOCK:     return "Red Block";
    case BlockType::ORANGE_BLOCK:  return "Orange Block";
    case BlockType::YELLOW_BLOCK:  return "Yellow Block";
    case BlockType::GREEN_BLOCK:   return "Green Block";
    case BlockType::BLUE_BLOCK:    return "Blue Block";
    case BlockType::PURPLE_BLOCK:  return "Purple Block";
    case BlockType::PINK_BLOCK:    return "Pink Block";
    case BlockType::GLOW_CRYSTAL:  return "Glow Crystal";
    case BlockType::CRAFTING_TABLE: return "Crafting Table";
    case BlockType::FURNACE:       return "Furnace";
    case BlockType::FURNACE_LIT:   return "Lit Furnace";
    case BlockType::CHEST:         return "Chest";
    case BlockType::TREASURE_CHEST: return "Treasure Chest";
    case BlockType::FARMLAND_DRY:  return "Farmland";
    case BlockType::FARMLAND_WET:  return "Wet Farmland";
    case BlockType::WHEAT_0: case BlockType::WHEAT_1:
    case BlockType::WHEAT_2: case BlockType::WHEAT_3: return "Wheat";
    case BlockType::CARROT_0: case BlockType::CARROT_1:
    case BlockType::CARROT_2: case BlockType::CARROT_3: return "Carrots";
    case BlockType::STRAWBERRY_0: case BlockType::STRAWBERRY_1:
    case BlockType::STRAWBERRY_2: case BlockType::STRAWBERRY_3: return "Strawberries";
    case BlockType::TALL_GRASS:    return "Tall Grass";
    case BlockType::POPPY:         return "Poppy";
    case BlockType::BUTTERCUP:     return "Buttercup";
    case BlockType::CORNFLOWER:    return "Cornflower";
    case BlockType::PINK_TULIP:    return "Pink Tulip";
    case BlockType::DAISY:         return "Daisy";
    case BlockType::LAVENDER:      return "Lavender";
    case BlockType::HIBISCUS:      return "Hibiscus";
    case BlockType::FERN:          return "Fern";
    case BlockType::RED_MUSHROOM:  return "Red Mushroom";
    case BlockType::BROWN_MUSHROOM: return "Brown Mushroom";
    case BlockType::DEAD_BUSH:     return "Dead Bush";
    case BlockType::BERRY_BUSH:
    case BlockType::BERRY_BUSH_BARE: return "Berry Bush";
    case BlockType::SHELLS:        return "Shells";
    case BlockType::CACTUS:        return "Cactus";
    case BlockType::PINK_CORAL:    return "Pink Coral";
    case BlockType::BLUE_CORAL:    return "Blue Coral";
    case BlockType::YELLOW_CORAL:  return "Yellow Coral";
    case BlockType::SANDSTONE_BRICKS: return "Sandstone Bricks";
    case BlockType::SNOW_BRICKS:   return "Snow Bricks";
    default:
      if (isTorch(type)) return "Torch";
      if (isCake(type)) return "Cake";
      if (isDoor(type)) return "Wooden Door";
      if (isBed(type)) return kBedNames[static_cast<int>(bedColour(type))];
      if (isSlab(type)) return kSlabNames[shapeMaterialIndex(type)];
      if (isStair(type)) return kStairNames[shapeMaterialIndex(type)];
      if (isFence(type)) return kFenceNames[fenceWood(type)];
      if (isGate(type)) return "Fence Gate";
      if (isLadder(type)) return "Ladder";
      return "Unknown";
  }
}

// Door tiles (tools/gen_building_tiles.py): the panel's faces, and its edges.
constexpr glm::ivec2 kDoorBottomTile{10, 3}; // with the handle
constexpr glm::ivec2 kDoorTopTile{11, 3};    // with the window
constexpr glm::ivec2 kDoorEdgeTile{12, 3};
// Bed tiles: the wooden frame, the pillow, then a blanket for each colour.
constexpr glm::ivec2 kBedFrameTile{13, 3};
constexpr glm::ivec2 kBedPillowTile{14, 3};
inline glm::ivec2 bedBlanketTile(BedColour colour) {
  int i = 15 + static_cast<int>(colour); // counted from (0, 3)
  return {i % 16, 3 + i / 16};
}

// Plant, cactus, coral, torch, cake and brick tiles, counted on from the
// last crop's at (8, 5) in tools/gen_building_tiles.py order.
inline glm::ivec2 tileAfterCrops(int n) {
  int i = 25 + n; // counted from (0, 4)
  return {i % 16, 4 + i / 16};
}
inline glm::ivec2 plantTile(BlockType plant) {
  return tileAfterCrops(static_cast<int>(plant) - static_cast<int>(BlockType::TALL_GRASS));
}
inline const glm::ivec2 kCactusTopTile = tileAfterCrops(15);
inline const glm::ivec2 kCactusSideTile = tileAfterCrops(16);
inline const glm::ivec2 kPinkCoralTile = tileAfterCrops(17);
inline const glm::ivec2 kBlueCoralTile = tileAfterCrops(18);
inline const glm::ivec2 kYellowCoralTile = tileAfterCrops(19);
inline const glm::ivec2 kTorchTile = tileAfterCrops(20);
inline const glm::ivec2 kCakeTopTile = tileAfterCrops(21);
inline const glm::ivec2 kCakeSideTile = tileAfterCrops(22);
inline const glm::ivec2 kCakeCutTile = tileAfterCrops(23);
inline const glm::ivec2 kSandstoneBricksTile = tileAfterCrops(24);
inline const glm::ivec2 kSnowBricksTile = tileAfterCrops(25);
inline const glm::ivec2 kLadderTile = tileAfterCrops(26);

// Shown flat in a slot (the plant's own picture), not as a little cube.
inline bool hasFlatIcon(BlockType type) { return isPlant(type) || isTorch(type) || isLadder(type); }

static robin_hood::unordered_map<BlockType, BlockTexture> block_data = [] {
  robin_hood::unordered_map<BlockType, BlockTexture> d = {
    {BlockType::GRASS, {glm::ivec2{0, 0}, glm::ivec2{2, 0}, glm::ivec2{1, 0}}},
    {BlockType::DIRT, {glm::ivec2{2, 0}, glm::ivec2{2, 0}, glm::ivec2{2, 0}}},
    {BlockType::STONE, {glm::ivec2{3, 0}, glm::ivec2{3, 0}, glm::ivec2{3, 0}}},
    {BlockType::BEDROCK, {glm::ivec2{4, 0}, glm::ivec2{4, 0}, glm::ivec2{4, 0}}},
    {BlockType::SAND, {glm::ivec2{5, 0}, glm::ivec2{5, 0}, glm::ivec2{5, 0}}},
    {BlockType::COBBLESTONE, {glm::ivec2{6, 0}, glm::ivec2{6, 0}, glm::ivec2{6, 0}}},
    {BlockType::WATER, {glm::ivec2{7, 0}, glm::ivec2{7, 0}, glm::ivec2{7, 0}}},
    {BlockType::SNOW, {glm::ivec2{8, 0}, glm::ivec2{8, 0}, glm::ivec2{8, 0}}},
    {BlockType::SNOW_STONE, {glm::ivec2{8, 0}, glm::ivec2{9, 0}, glm::ivec2{9, 0}}},
    {BlockType::SNOW_DIRT, {glm::ivec2{8, 0}, glm::ivec2{10, 0}, glm::ivec2{10, 0}}},
    {BlockType::SANDSTONE, {glm::ivec2{11, 0}, glm::ivec2{11, 0}, glm::ivec2{11, 0}}},
    {BlockType::OAK_LOG, {glm::ivec2{14, 0}, glm::ivec2{14, 0}, glm::ivec2{13, 0}}},
    {BlockType::OAK_LEAF, {glm::ivec2{12, 0}, glm::ivec2{12, 0}, glm::ivec2{12, 0}}},
    {BlockType::PINE_LOG, {glm::ivec2{0, 1}, glm::ivec2{0, 1}, glm::ivec2{15, 0}}},
    {BlockType::PINE_LEAF, {glm::ivec2{1, 1}, glm::ivec2{1, 1}, glm::ivec2{1, 1}}},
    {BlockType::PALM_LOG, {glm::ivec2{3, 1}, glm::ivec2{3, 1}, glm::ivec2{2, 1}}},
    {BlockType::PALM_LEAF, {glm::ivec2{4, 1}, glm::ivec2{4, 1}, glm::ivec2{4, 1}}},
    {BlockType::COAL_ORE, {glm::ivec2{5, 1}, glm::ivec2{5, 1}, glm::ivec2{5, 1}}},
    {BlockType::GOLD_ORE, {glm::ivec2{6, 1}, glm::ivec2{6, 1}, glm::ivec2{6, 1}}},
    {BlockType::DIAMOND_ORE, {glm::ivec2{7, 1}, glm::ivec2{7, 1}, glm::ivec2{7, 1}}},
    {BlockType::IRON_ORE, {glm::ivec2{8, 1}, glm::ivec2{8, 1}, glm::ivec2{8, 1}}},
    {BlockType::EMERALD_ORE, {glm::ivec2{9, 1}, glm::ivec2{9, 1}, glm::ivec2{9, 1}}},
    {BlockType::RUBY_ORE, {glm::ivec2{10, 1}, glm::ivec2{10, 1}, glm::ivec2{10, 1}}},
    {BlockType::COPPER_ORE, {glm::ivec2{11, 1}, glm::ivec2{11, 1}, glm::ivec2{11, 1}}},
    {BlockType::CHERRY_LEAF, {glm::ivec2{12, 1}, glm::ivec2{12, 1}, glm::ivec2{12, 1}}},
    {BlockType::CHERRY_LOG, {glm::ivec2{14, 1}, glm::ivec2{14, 1}, glm::ivec2{13, 1}}},
    // Building blocks fill row 2 on, in this order (tools/gen_building_tiles.py).
    {BlockType::OAK_PLANKS, {glm::ivec2{0, 2}, glm::ivec2{0, 2}, glm::ivec2{0, 2}}},
    {BlockType::PINE_PLANKS, {glm::ivec2{1, 2}, glm::ivec2{1, 2}, glm::ivec2{1, 2}}},
    {BlockType::PALM_PLANKS, {glm::ivec2{2, 2}, glm::ivec2{2, 2}, glm::ivec2{2, 2}}},
    {BlockType::CHERRY_PLANKS, {glm::ivec2{3, 2}, glm::ivec2{3, 2}, glm::ivec2{3, 2}}},
    {BlockType::BRICKS, {glm::ivec2{4, 2}, glm::ivec2{4, 2}, glm::ivec2{4, 2}}},
    {BlockType::STONE_BRICKS, {glm::ivec2{5, 2}, glm::ivec2{5, 2}, glm::ivec2{5, 2}}},
    {BlockType::GLASS, {glm::ivec2{6, 2}, glm::ivec2{6, 2}, glm::ivec2{6, 2}}},
    {BlockType::WHITE_BLOCK, {glm::ivec2{7, 2}, glm::ivec2{7, 2}, glm::ivec2{7, 2}}},
    {BlockType::RED_BLOCK, {glm::ivec2{8, 2}, glm::ivec2{8, 2}, glm::ivec2{8, 2}}},
    {BlockType::ORANGE_BLOCK, {glm::ivec2{9, 2}, glm::ivec2{9, 2}, glm::ivec2{9, 2}}},
    {BlockType::YELLOW_BLOCK, {glm::ivec2{10, 2}, glm::ivec2{10, 2}, glm::ivec2{10, 2}}},
    {BlockType::GREEN_BLOCK, {glm::ivec2{11, 2}, glm::ivec2{11, 2}, glm::ivec2{11, 2}}},
    {BlockType::BLUE_BLOCK, {glm::ivec2{12, 2}, glm::ivec2{12, 2}, glm::ivec2{12, 2}}},
    {BlockType::PURPLE_BLOCK, {glm::ivec2{13, 2}, glm::ivec2{13, 2}, glm::ivec2{13, 2}}},
    {BlockType::PINK_BLOCK, {glm::ivec2{14, 2}, glm::ivec2{14, 2}, glm::ivec2{14, 2}}},
    {BlockType::GLOWSTONE, {glm::ivec2{15, 2}, glm::ivec2{15, 2}, glm::ivec2{15, 2}}},
    // Shard tips seen end-on above and below, the shards themselves round the
    // sides (drawn upside down when the crystal hangs; see chunk_mesh.cpp).
    {BlockType::GLOW_CRYSTAL, {glm::ivec2{7, 4}, glm::ivec2{7, 4}, glm::ivec2{0, 3}}},
    // A worktop on top, a tool rack round the sides, and the worktop again below.
    {BlockType::CRAFTING_TABLE, {glm::ivec2{1, 3}, glm::ivec2{1, 3}, glm::ivec2{2, 3}}},
    // A stone top and a mouth on every side; flames in it while lit.
    {BlockType::FURNACE, {glm::ivec2{3, 3}, glm::ivec2{3, 3}, glm::ivec2{4, 3}}},
    {BlockType::FURNACE_LIT, {glm::ivec2{3, 3}, glm::ivec2{3, 3}, glm::ivec2{5, 3}}},
    // A lid on top and a latch on every side; the treasure chest in dark
    // wood with gold bands and a lock.
    {BlockType::CHEST, {glm::ivec2{6, 3}, glm::ivec2{6, 3}, glm::ivec2{7, 3}}},
    {BlockType::TREASURE_CHEST, {glm::ivec2{8, 3}, glm::ivec2{8, 3}, glm::ivec2{9, 3}}},
  };
  // Doors: edges on top and bottom, the half's face round the sides.
  for (int v = static_cast<int>(BlockType::DOOR_BOTTOM_N_CLOSED);
       v <= static_cast<int>(BlockType::DOOR_TOP_W_OPEN); ++v) {
    BlockType b = static_cast<BlockType>(v);
    d[b] = {kDoorEdgeTile, kDoorEdgeTile, doorTop(b) ? kDoorTopTile : kDoorBottomTile};
  }
  // Beds: the blanket (or the pillow at the head) on top, the frame below.
  for (int v = static_cast<int>(BlockType::BED_WHITE_FOOT_N);
       v <= static_cast<int>(BlockType::BED_PINK_HEAD_W); ++v) {
    BlockType b = static_cast<BlockType>(v);
    d[b] = {bedHead(b) ? kBedPillowTile : bedBlanketTile(bedColour(b)), kBedFrameTile,
            kBedFrameTile};
  }
  // Farmland: furrows on top, dirt below; darker once watered.
  d[BlockType::FARMLAND_DRY] = {kFarmlandDryTop, glm::ivec2{2, 0}, kFarmlandDrySide};
  d[BlockType::FARMLAND_WET] = {kFarmlandWetTop, glm::ivec2{2, 0}, kFarmlandWetSide};
  // Crops: the plant round the sides, nothing on top or below.
  for (int v = static_cast<int>(BlockType::WHEAT_0);
       v <= static_cast<int>(BlockType::STRAWBERRY_3); ++v) {
    BlockType b = static_cast<BlockType>(v);
    d[b] = {kCropTopTile, kCropTopTile, cropTile(b)};
  }
  // Plants: the plant round the sides, clear on top and below (like crops);
  // then cactus, corals, torches, cake and the bricks (tools/gen_building_tiles.py).
  for (int v = static_cast<int>(BlockType::TALL_GRASS);
       v <= static_cast<int>(BlockType::SHELLS); ++v)
    d[static_cast<BlockType>(v)] = {kCropTopTile, kCropTopTile,
                                    plantTile(static_cast<BlockType>(v))};
  d[BlockType::CACTUS] = {kCactusTopTile, kCactusTopTile, kCactusSideTile};
  d[BlockType::PINK_CORAL] = {kPinkCoralTile, kPinkCoralTile, kPinkCoralTile};
  d[BlockType::BLUE_CORAL] = {kBlueCoralTile, kBlueCoralTile, kBlueCoralTile};
  d[BlockType::YELLOW_CORAL] = {kYellowCoralTile, kYellowCoralTile, kYellowCoralTile};
  for (int v = static_cast<int>(BlockType::TORCH); v <= static_cast<int>(BlockType::TORCH_W); ++v)
    d[static_cast<BlockType>(v)] = {kCropTopTile, kCropTopTile, kTorchTile};
  // A cake's top, its side, and where it's been cut (the "bottom" slot).
  for (int v = static_cast<int>(BlockType::CAKE_4); v <= static_cast<int>(BlockType::CAKE_1); ++v)
    d[static_cast<BlockType>(v)] = {kCakeTopTile, kCakeCutTile, kCakeSideTile};
  d[BlockType::SANDSTONE_BRICKS] = {kSandstoneBricksTile, kSandstoneBricksTile,
                                    kSandstoneBricksTile};
  d[BlockType::SNOW_BRICKS] = {kSnowBricksTile, kSnowBricksTile, kSnowBricksTile};
  // Slabs and stairs wear their material's tiles.
  for (int v = static_cast<int>(BlockType::OAK_SLAB);
       v <= static_cast<int>(BlockType::SNOW_BRICK_STAIRS_W); ++v) {
    const BlockTexture material = d.at(shapeMaterial(static_cast<BlockType>(v))); // a copy: d may grow
    d[static_cast<BlockType>(v)] = material;
  }
  // Fences in their wood's planks, the gate in oak; ladders their own tile.
  const BlockType fence_planks[kFenceWoods] = {BlockType::OAK_PLANKS, BlockType::PINE_PLANKS,
                                               BlockType::PALM_PLANKS, BlockType::CHERRY_PLANKS};
  for (int w = 0; w < kFenceWoods; ++w) {
    const BlockTexture planks = d.at(fence_planks[w]); // a copy: d may grow
    d[static_cast<BlockType>(static_cast<int>(BlockType::OAK_FENCE) + w)] = planks;
  }
  const BlockTexture oak = d.at(BlockType::OAK_PLANKS);
  for (int v = static_cast<int>(BlockType::FENCE_GATE_X_CLOSED);
       v <= static_cast<int>(BlockType::FENCE_GATE_Z_OPEN); ++v)
    d[static_cast<BlockType>(v)] = oak;
  for (int v = static_cast<int>(BlockType::LADDER_N); v <= static_cast<int>(BlockType::LADDER_W); ++v)
    d[static_cast<BlockType>(v)] = {kLadderTile, kLadderTile, kLadderTile};
  return d;
}();
