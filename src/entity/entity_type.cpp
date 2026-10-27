#include "entity/entity_type.hpp"
#include "entity/brain.hpp"

namespace {

std::unique_ptr<Brain> makeWander() { return std::make_unique<WanderBrain>(); }

std::unique_ptr<Brain> makeGrazer() {
  WanderOptions o;
  o.cautious = true;
  return std::make_unique<WanderBrain>(o);
}
std::unique_ptr<Brain> makeHopper() {
  WanderOptions o;
  o.cautious = true;
  o.hop = true;
  return std::make_unique<WanderBrain>(o);
}
std::unique_ptr<Brain> makeSwimmer() {
  WanderOptions o;
  o.cautious = true;
  o.can_swim = true;
  return std::make_unique<WanderBrain>(o);
}

std::unique_ptr<Brain> makeSwimBrain() { return std::make_unique<SwimBrain>(); }
std::unique_ptr<Brain> makeChaser() { return std::make_unique<ChaseBrain>(); }
std::unique_ptr<Brain> makeHoppingChaser() {
  ChaseOptions o;
  o.hop = true;
  return std::make_unique<ChaseBrain>(o);
}

// Appends four legs of the given size, inset at the corners of a footprint
// half_x wide and spanning z_front..z_back, swinging in diagonal pairs.
void addLegs(std::vector<ModelBox> &model, float half_x, float z_front, float z_back,
             glm::vec3 size, glm::vec3 color) {
  float left = -half_x, right = half_x - size.x;
  float front = z_front, back = z_back - size.z;
  model.push_back({{left, 0.0f, front}, size, color, +1});
  model.push_back({{right, 0.0f, front}, size, color, -1});
  model.push_back({{left, 0.0f, back}, size, color, -1});
  model.push_back({{right, 0.0f, back}, size, color, +1});
}

const glm::vec3 kEyeBlack{0.05f, 0.05f, 0.05f};

const EntityTypeDef kDroppedItem{
    "Dropped Item",
    /*half_width=*/0.125f, /*height=*/0.25f,
    /*max_health=*/0,
    /*despawn_age_ms=*/5 * 60 * 1000,
    /*walk_speed=*/0.0f,
    /*model=*/{}, // drawn from the item's own art
    /*brain=*/nullptr,
    /*drops=*/{},
    /*animal=*/std::nullopt,
};

// A small, clearly-placeholder critter: a body, a head and two eyes.
const EntityTypeDef kTestCreature{
    "Test Creature",
    /*half_width=*/0.35f, /*height=*/0.9f,
    /*max_health=*/10,
    /*despawn_age_ms=*/0,
    /*walk_speed=*/2.0f,
    /*model=*/{
        {{-0.35f, 0.1f, -0.30f}, {0.70f, 0.50f, 0.80f}, {0.62f, 0.36f, 0.85f}}, // body
        {{-0.25f, 0.45f, -0.60f}, {0.50f, 0.45f, 0.40f}, {0.72f, 0.48f, 0.95f}}, // head
        {{-0.18f, 0.70f, -0.62f}, {0.10f, 0.10f, 0.03f}, kEyeBlack},             // eye
        {{0.08f, 0.70f, -0.62f}, {0.10f, 0.10f, 0.03f}, kEyeBlack},              // eye
        {{-0.30f, 0.0f, -0.25f}, {0.15f, 0.12f, 0.15f}, {0.30f, 0.18f, 0.45f}},  // feet
        {{0.15f, 0.0f, -0.25f}, {0.15f, 0.12f, 0.15f}, {0.30f, 0.18f, 0.45f}},
        {{-0.30f, 0.0f, 0.30f}, {0.15f, 0.12f, 0.15f}, {0.30f, 0.18f, 0.45f}},
        {{0.15f, 0.0f, 0.30f}, {0.15f, 0.12f, 0.15f}, {0.30f, 0.18f, 0.45f}},
    },
    /*brain=*/makeWander,
    /*drops=*/{{ItemId::COAL, 1}},
    /*animal=*/std::nullopt,
};

// ─── Animals ───────────────────────────────────────────────────────────
// Models face -Z (head toward -Z). Colours are sRGB.

EntityTypeDef makeWoolly() {
  const glm::vec3 wool{0.95f, 0.95f, 0.92f}, face{0.25f, 0.22f, 0.22f};
  EntityTypeDef d{"Woolly", 0.45f, 1.1f, 0, 0, 1.4f, {}, makeGrazer, {}, {}};
  d.model = {
      {{-0.45f, 0.30f, -0.40f}, {0.90f, 0.60f, 0.85f}, wool},       // fleece
      {{-0.20f, 0.55f, -0.68f}, {0.40f, 0.38f, 0.32f}, face},       // head
      {{-0.24f, 0.78f, -0.56f}, {0.48f, 0.18f, 0.20f}, wool},       // fluffy cap
      {{-0.13f, 0.72f, -0.69f}, {0.07f, 0.07f, 0.02f}, {1, 1, 1}},  // eyes
      {{0.06f, 0.72f, -0.69f}, {0.07f, 0.07f, 0.02f}, {1, 1, 1}},
  };
  addLegs(d.model, 0.36f, -0.32f, 0.38f, {0.18f, 0.32f, 0.18f}, face);
  d.animal = AnimalDef{itemForBlock(BlockType::GRASS), {ItemId::WOOL, 1},
                       {BiomeType::MOUNTAIN, BiomeType::PLAINS},
                       {BlockType::GRASS, BlockType::SNOW, BlockType::SNOW_DIRT}};
  return d;
}

EntityTypeDef makeBunny() {
  const glm::vec3 fur{0.80f, 0.66f, 0.50f}, pink{0.95f, 0.65f, 0.72f};
  EntityTypeDef d{"Bunny", 0.2f, 0.5f, 0, 0, 2.6f, {}, makeHopper, {}, {}};
  d.model = {
      {{-0.17f, 0.08f, -0.15f}, {0.34f, 0.24f, 0.36f}, fur},          // body
      {{-0.12f, 0.20f, -0.33f}, {0.24f, 0.20f, 0.20f}, fur},          // head
      {{-0.11f, 0.40f, -0.26f}, {0.06f, 0.20f, 0.04f}, fur},          // ears
      {{0.05f, 0.40f, -0.26f}, {0.06f, 0.20f, 0.04f}, fur},
      {{-0.10f, 0.42f, -0.27f}, {0.04f, 0.15f, 0.01f}, pink},
      {{0.06f, 0.42f, -0.27f}, {0.04f, 0.15f, 0.01f}, pink},
      {{-0.05f, 0.16f, 0.20f}, {0.10f, 0.10f, 0.06f}, {1, 1, 1}},     // tail
      {{-0.09f, 0.31f, -0.34f}, {0.05f, 0.05f, 0.02f}, kEyeBlack},    // eyes
      {{0.04f, 0.31f, -0.34f}, {0.05f, 0.05f, 0.02f}, kEyeBlack},
  };
  addLegs(d.model, 0.15f, -0.13f, 0.20f, {0.09f, 0.10f, 0.12f}, fur);
  d.animal = AnimalDef{itemForBlock(BlockType::CHERRY_LEAF), {ItemId::BERRIES, 1},
                       {BiomeType::PLAINS, BiomeType::CHERRY},
                       {BlockType::GRASS}};
  return d;
}

EntityTypeDef makeShellTurtle() {
  const glm::vec3 shell{0.25f, 0.50f, 0.30f}, ridge{0.16f, 0.36f, 0.20f},
      skin{0.58f, 0.72f, 0.46f};
  EntityTypeDef d{"Shell Turtle", 0.4f, 0.5f, 0, 0, 0.7f, {}, makeSwimmer, {}, {}};
  d.model = {
      {{-0.40f, 0.12f, -0.35f}, {0.80f, 0.28f, 0.70f}, shell},        // shell
      {{-0.30f, 0.40f, -0.25f}, {0.60f, 0.08f, 0.50f}, ridge},        // shell top
      {{-0.12f, 0.12f, -0.58f}, {0.24f, 0.18f, 0.24f}, skin},         // head
      {{-0.08f, 0.24f, -0.59f}, {0.05f, 0.05f, 0.02f}, kEyeBlack},    // eyes
      {{0.03f, 0.24f, -0.59f}, {0.05f, 0.05f, 0.02f}, kEyeBlack},
  };
  addLegs(d.model, 0.42f, -0.30f, 0.32f, {0.16f, 0.12f, 0.16f}, skin);
  d.animal = AnimalDef{itemForBlock(BlockType::PALM_LEAF), {ItemId::SEA_SHELL, 1},
                       {BiomeType::TROPICAL}, {BlockType::SAND}};
  return d;
}

EntityTypeDef makeBlossomDeer() {
  const glm::vec3 coat{0.72f, 0.46f, 0.32f}, leg{0.52f, 0.33f, 0.22f},
      blossom{0.96f, 0.62f, 0.78f};
  EntityTypeDef d{"Blossom Deer", 0.4f, 1.5f, 0, 0, 2.2f, {}, makeGrazer, {}, {}};
  d.model = {
      {{-0.28f, 0.70f, -0.45f}, {0.56f, 0.42f, 0.90f}, coat},          // body
      {{0.27f, 0.85f, -0.20f}, {0.02f, 0.12f, 0.12f}, blossom},        // spots
      {{0.27f, 0.80f, 0.15f}, {0.02f, 0.10f, 0.10f}, blossom},
      {{-0.29f, 0.88f, -0.05f}, {0.02f, 0.12f, 0.12f}, blossom},
      {{-0.12f, 0.95f, -0.58f}, {0.24f, 0.36f, 0.20f}, coat},          // neck
      {{-0.14f, 1.20f, -0.78f}, {0.28f, 0.25f, 0.34f}, coat},          // head
      {{-0.12f, 1.45f, -0.60f}, {0.05f, 0.16f, 0.05f}, leg},           // antlers
      {{0.07f, 1.45f, -0.60f}, {0.05f, 0.16f, 0.05f}, leg},
      {{-0.15f, 1.58f, -0.63f}, {0.11f, 0.08f, 0.11f}, blossom},       // blossoms
      {{0.04f, 1.58f, -0.63f}, {0.11f, 0.08f, 0.11f}, blossom},
      {{-0.10f, 1.33f, -0.79f}, {0.05f, 0.05f, 0.02f}, kEyeBlack},     // eyes
      {{0.05f, 1.33f, -0.79f}, {0.05f, 0.05f, 0.02f}, kEyeBlack},
  };
  addLegs(d.model, 0.26f, -0.40f, 0.40f, {0.12f, 0.70f, 0.12f}, leg);
  d.animal = AnimalDef{itemForBlock(BlockType::OAK_LEAF), {ItemId::SWEET_PETALS, 1},
                       {BiomeType::CHERRY}, {BlockType::GRASS}};
  return d;
}

// ─── Enemies ───────────────────────────────────────────────────────────
// Spooky but cute. Eyes glow so they read in the dark.

ItemStack glowstone(uint16_t n) { return {itemForBlock(BlockType::GLOWSTONE), n}; }
const ChanceDrop kSometimesCoal{{ItemId::COAL, 1}, 0.5f};

EntityTypeDef makeGloomling() {
  const glm::vec3 shadow{0.20f, 0.13f, 0.30f}, wisp{0.30f, 0.20f, 0.42f},
      eye{1.00f, 0.88f, 0.35f};
  EntityTypeDef d{"Gloomling", 0.3f, 0.6f, 6, 0, 3.0f, {}, makeHoppingChaser,
                  {glowstone(1)}, std::nullopt};
  d.model = {
      {{-0.30f, 0.00f, -0.30f}, {0.60f, 0.48f, 0.60f}, shadow},           // blob
      {{-0.22f, 0.48f, -0.22f}, {0.44f, 0.10f, 0.44f}, wisp},             // crown
      {{-0.08f, 0.58f, -0.06f}, {0.14f, 0.10f, 0.14f}, wisp},             // wisp tip
      {{-0.18f, 0.26f, -0.31f}, {0.12f, 0.12f, 0.02f}, eye, 0, true},     // eyes
      {{0.06f, 0.26f, -0.31f}, {0.12f, 0.12f, 0.02f}, eye, 0, true},
  };
  d.chance_drops = {kSometimesCoal};
  d.enemy = EnemyDef{/*damage=*/1, /*surface=*/true, /*vanish_at_dawn=*/true, {}};
  return d;
}

EntityTypeDef makeThornCrawler() {
  const glm::vec3 shell{0.30f, 0.38f, 0.16f}, thorn{0.52f, 0.36f, 0.20f},
      leg{0.22f, 0.26f, 0.12f}, eye{1.00f, 0.45f, 0.20f};
  EntityTypeDef d{"Thorn Crawler", 0.35f, 0.5f, 4, 0, 4.2f, {}, makeChaser,
                  {glowstone(1)}, std::nullopt};
  d.model = {
      {{-0.33f, 0.14f, -0.30f}, {0.66f, 0.24f, 0.70f}, shell},            // body
      {{-0.18f, 0.12f, -0.45f}, {0.36f, 0.20f, 0.16f}, shell},            // head
      {{-0.25f, 0.38f, -0.20f}, {0.06f, 0.10f, 0.06f}, thorn},            // thorns
      {{0.15f, 0.38f, -0.10f}, {0.06f, 0.10f, 0.06f}, thorn},
      {{-0.05f, 0.38f, 0.05f}, {0.06f, 0.12f, 0.06f}, thorn},
      {{-0.20f, 0.38f, 0.25f}, {0.06f, 0.08f, 0.06f}, thorn},
      {{0.18f, 0.38f, 0.28f}, {0.06f, 0.10f, 0.06f}, thorn},
      {{-0.13f, 0.22f, -0.46f}, {0.07f, 0.06f, 0.02f}, eye, 0, true},     // eyes
      {{0.06f, 0.22f, -0.46f}, {0.07f, 0.06f, 0.02f}, eye, 0, true},
  };
  addLegs(d.model, 0.35f, -0.26f, 0.36f, {0.10f, 0.16f, 0.10f}, leg);
  d.chance_drops = {kSometimesCoal};
  d.enemy = EnemyDef{/*damage=*/2, /*surface=*/true, /*vanish_at_dawn=*/true,
                     {BiomeType::PLAINS, BiomeType::MOUNTAIN, BiomeType::CHERRY}};
  return d;
}

EntityTypeDef makeCaveGrumble() {
  const glm::vec3 rock{0.45f, 0.45f, 0.48f}, dark{0.32f, 0.32f, 0.35f},
      moss{0.30f, 0.52f, 0.22f}, eye{0.45f, 0.95f, 1.00f};
  EntityTypeDef d{"Cave Grumble", 0.4f, 1.3f, 16, 0, 1.6f, {}, makeChaser,
                  {glowstone(2)}, std::nullopt};
  d.model = {
      {{-0.40f, 0.30f, -0.32f}, {0.80f, 0.80f, 0.64f}, rock},             // body
      {{-0.42f, 1.10f, -0.34f}, {0.84f, 0.14f, 0.68f}, moss},             // mossy top
      {{-0.20f, 1.24f, -0.10f}, {0.18f, 0.06f, 0.18f}, moss},
      {{-0.26f, 0.88f, -0.34f}, {0.52f, 0.08f, 0.03f}, dark},             // brow
      {{-0.22f, 0.76f, -0.34f}, {0.12f, 0.10f, 0.02f}, eye, 0, true},     // eyes
      {{0.10f, 0.76f, -0.34f}, {0.12f, 0.10f, 0.02f}, eye, 0, true},
      {{-0.54f, 0.40f, -0.12f}, {0.14f, 0.50f, 0.24f}, dark},             // arms
      {{0.40f, 0.40f, -0.12f}, {0.14f, 0.50f, 0.24f}, dark},
      {{-0.30f, 0.00f, -0.15f}, {0.24f, 0.30f, 0.30f}, dark, +1},         // legs
      {{0.06f, 0.00f, -0.15f}, {0.24f, 0.30f, 0.30f}, dark, -1},
  };
  d.chance_drops = {kSometimesCoal};
  d.enemy = EnemyDef{/*damage=*/2, /*surface=*/false, /*vanish_at_dawn=*/false, {}};
  return d;
}

// ─── Fish ──────────────────────────────────────────────────────────────
// Small, facing -Z, with a tail that wags while swimming. Spots are on both
// flanks; glowing ones are drawn unlit.

struct FishLook {
  glm::vec3 body, dark, fin, spot;
  bool glowing_spots = false;
};

EntityTypeDef makeFish(std::string_view name, float half_width, float height,
                       const FishLook &look, FishDef fish) {
  EntityTypeDef d{name, half_width, height, 0, 0, 1.5f, {}, makeSwimBrain, {}, {}};
  const float w = half_width * 2.0f * 0.6f, len = half_width * 2.0f * 1.4f;
  const float x0 = -w / 2, z0 = -len / 2, h = height * 0.8f, y0 = height * 0.1f;
  d.model = {
      {{x0, y0, z0}, {w, h, len}, look.body},                                      // body
      {{x0 + w * 0.1f, y0 + h, z0 + len * 0.3f}, {w * 0.8f, height * 0.12f, len * 0.35f},
       look.fin},                                                                  // top fin
      {{x0 * 0.4f, y0 + h * 0.15f, z0 + len}, {w * 0.4f, h * 0.7f, len * 0.35f}, look.fin,
       0, false, true},                                                            // tail (wags)
      {{x0 - 0.005f, y0 + h * 0.55f, z0 + len * 0.1f}, {0.03f, 0.05f, 0.05f}, kEyeBlack},
      {{-x0 - 0.025f, y0 + h * 0.55f, z0 + len * 0.1f}, {0.03f, 0.05f, 0.05f}, kEyeBlack},
  };
  // Spots on each flank.
  for (float z : {0.35f, 0.6f})
    for (float side : {-1.0f, 1.0f}) {
      float x = side < 0 ? x0 - 0.005f : -x0 - 0.025f;
      d.model.push_back({{x, y0 + h * 0.3f, z0 + len * z}, {0.03f, h * 0.25f, len * 0.1f},
                         look.spot, 0, look.glowing_spots});
    }
  d.fish = std::move(fish);
  return d;
}

const EntityTypeDef kPebblefish = makeFish(
    "Pebblefish", 0.2f, 0.3f,
    {{0.49f, 0.59f, 0.47f}, {0.25f, 0.31f, 0.24f}, {0.67f, 0.73f, 0.59f}, {0.36f, 0.42f, 0.34f}},
    {{BiomeType::PLAINS, BiomeType::MOUNTAIN, BiomeType::CHERRY, BiomeType::DESERT}, false,
     ItemId::RAW_PEBBLEFISH});
const EntityTypeDef kSunfin = makeFish(
    "Sunfin", 0.25f, 0.4f,
    {{0.96f, 0.55f, 0.16f}, {0.59f, 0.27f, 0.04f}, {1.0f, 0.84f, 0.24f}, {1.0f, 0.84f, 0.24f}},
    {{BiomeType::OCEAN, BiomeType::TROPICAL}, false, ItemId::RAW_SUNFIN});
const EntityTypeDef kGlowfin = makeFish(
    "Glowfin", 0.2f, 0.3f,
    {{0.14f, 0.24f, 0.51f}, {0.05f, 0.09f, 0.25f}, {0.20f, 0.34f, 0.62f}, {0.35f, 0.94f, 0.94f},
     true},
    {{}, true, ItemId::RAW_GLOWFIN});

// Red below, white on top. The Game moves it: no brain.
const EntityTypeDef kBobber{
    "Bobber", 0.1f, 0.2f, 0, 0, 0.0f,
    {{{-0.1f, 0.0f, -0.1f}, {0.2f, 0.1f, 0.2f}, {0.86f, 0.16f, 0.16f}},
     {{-0.1f, 0.1f, -0.1f}, {0.2f, 0.08f, 0.2f}, {0.96f, 0.96f, 0.96f}},
     {{-0.02f, 0.18f, -0.02f}, {0.04f, 0.06f, 0.04f}, {0.86f, 0.16f, 0.16f}}},
    nullptr, {}, {}};

const EntityTypeDef kGloomling = makeGloomling();
const EntityTypeDef kThornCrawler = makeThornCrawler();
const EntityTypeDef kCaveGrumble = makeCaveGrumble();

const EntityTypeDef kWoolly = makeWoolly();
const EntityTypeDef kBunny = makeBunny();
const EntityTypeDef kShellTurtle = makeShellTurtle();
const EntityTypeDef kBlossomDeer = makeBlossomDeer();

} // namespace

bool isValidEntityKind(EntityKind kind) {
  switch (kind) {
    case EntityKind::DroppedItem:
    case EntityKind::TestCreature:
    case EntityKind::Woolly:
    case EntityKind::Bunny:
    case EntityKind::ShellTurtle:
    case EntityKind::BlossomDeer:
    case EntityKind::Gloomling:
    case EntityKind::ThornCrawler:
    case EntityKind::CaveGrumble:
    case EntityKind::Pebblefish:
    case EntityKind::Sunfin:
    case EntityKind::Glowfin:
    case EntityKind::Bobber:       return true;
  }
  return false;
}

const EntityTypeDef &entityType(EntityKind kind) {
  switch (kind) {
    case EntityKind::DroppedItem:  return kDroppedItem;
    case EntityKind::TestCreature: return kTestCreature;
    case EntityKind::Woolly:       return kWoolly;
    case EntityKind::Bunny:        return kBunny;
    case EntityKind::ShellTurtle:  return kShellTurtle;
    case EntityKind::BlossomDeer:  return kBlossomDeer;
    case EntityKind::Gloomling:    return kGloomling;
    case EntityKind::ThornCrawler: return kThornCrawler;
    case EntityKind::CaveGrumble:  return kCaveGrumble;
    case EntityKind::Pebblefish:   return kPebblefish;
    case EntityKind::Sunfin:       return kSunfin;
    case EntityKind::Glowfin:      return kGlowfin;
    case EntityKind::Bobber:       return kBobber;
  }
  return kTestCreature;
}
