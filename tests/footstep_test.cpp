#include "doctest/doctest.h"
#include "audio/step_surface.hpp"
#include "block/bed.hpp"
#include "block/stairs.hpp"

TEST_CASE("every block has a footstep set") {
  for (int v = 0; v <= static_cast<int>(kLastBlockType); ++v) {
    CAPTURE(v);
    CHECK(listedStepSurface(static_cast<BlockType>(v)).has_value());
  }
}

TEST_CASE("footstep sets for each kind of surface") {
  CHECK(stepSurface(BlockType::GRASS) == StepSet::Grass);
  CHECK(stepSurface(BlockType::OAK_LEAF) == StepSet::Grass);
  CHECK(stepSurface(BlockType::TALL_GRASS) == StepSet::Grass);
  CHECK(stepSurface(BlockType::DIRT) == StepSet::Dirt);
  CHECK(stepSurface(BlockType::SAND) == StepSet::Dirt);
  CHECK(stepSurface(BlockType::FARMLAND_WET) == StepSet::Dirt);
  CHECK(stepSurface(BlockType::WHEAT_2) == StepSet::Dirt);
  CHECK(stepSurface(BlockType::SHELLS) == StepSet::Dirt);
  CHECK(stepSurface(BlockType::STONE) == StepSet::Stone);
  CHECK(stepSurface(BlockType::DIAMOND_ORE) == StepSet::Stone);
  CHECK(stepSurface(BlockType::GLASS) == StepSet::Stone);
  CHECK(stepSurface(BlockType::OAK_PLANKS) == StepSet::Wood);
  CHECK(stepSurface(BlockType::PINE_LOG) == StepSet::Wood);
  CHECK(stepSurface(BlockType::LADDER_E) == StepSet::Wood);
  CHECK(stepSurface(BlockType::FENCE_GATE_Z_OPEN) == StepSet::Wood);
  CHECK(stepSurface(BlockType::DOOR_TOP_W_OPEN) == StepSet::Wood);
  CHECK(stepSurface(BlockType::SNOW) == StepSet::Snow);
  CHECK(stepSurface(BlockType::SNOW_DIRT) == StepSet::Snow);
  CHECK(stepSurface(BlockType::RED_BLOCK) == StepSet::Cloth);
  CHECK(stepSurface(BlockType::BED_PINK_HEAD_W) == StepSet::Cloth);
  CHECK(stepSurface(BlockType::CAKE_2) == StepSet::Cloth);
  CHECK(stepSurface(BlockType::AIR) == StepSet::Silent);
  CHECK(stepSurface(BlockType::WATER) == StepSet::Silent);
  CHECK(stepSurface(BlockType::TORCH_N) == StepSet::Silent);
}

TEST_CASE("slabs and stairs sound like what they're made of") {
  CHECK(stepSurface(BlockType::OAK_SLAB) == StepSet::Wood);
  CHECK(stepSurface(BlockType::CHERRY_STAIRS_S) == StepSet::Wood);
  CHECK(stepSurface(BlockType::COBBLESTONE_SLAB) == StepSet::Stone);
  CHECK(stepSurface(BlockType::BRICK_STAIRS_N) == StepSet::Stone);
  CHECK(stepSurface(BlockType::SANDSTONE_BRICK_SLAB) == StepSet::Stone);
  CHECK(stepSurface(BlockType::SNOW_BRICK_STAIRS_W) == StepSet::Snow);
}

// ─── FootstepDirector ──────────────────────────────────────────────────

#include "audio/footstep_director.hpp"
#include <set>

namespace {

constexpr float kDt = 1.0f / 60.0f;

Walker me(glm::vec3 pos, StepSet ground = StepSet::Grass) {
  Walker w;
  w.id = 1;
  w.local = true;
  w.position = pos;
  w.on_ground = true;
  w.ground = ground;
  return w;
}

std::vector<SoundEvent> tick(FootstepDirector &d, const Walker &w, glm::vec3 listener = {}) {
  return d.update({kDt, listener, {w}});
}

// Walks `w` along +x at `speed` for `seconds`; returns every sound and the
// distance walked when each played.
struct Walked { std::vector<SoundEvent> sounds; std::vector<float> at; };
Walked walk(FootstepDirector &d, Walker &w, float speed, float seconds, glm::vec3 listener = {}) {
  Walked r;
  const glm::vec3 start = w.position;
  for (float t = 0; t < seconds; t += kDt) {
    w.position.x += speed * kDt;
    for (const SoundEvent &e : tick(d, w, listener)) {
      r.sounds.push_back(e);
      r.at.push_back(w.position.x - start.x);
    }
  }
  return r;
}

} // namespace

TEST_CASE("no steps standing still or turning on the spot") {
  FootstepDirector d(1);
  Walker w = me({0, 64, 0});
  for (int i = 0; i < 600; ++i) CHECK(tick(d, w).empty());
}

TEST_CASE("the first step comes after half a block, then every stride") {
  FootstepDirector d(1);
  Walker w = me({0, 64, 0});
  tick(d, w);
  Walked r = walk(d, w, 4.317f, 3.0f);
  REQUIRE(r.at.size() >= 4);
  CHECK(r.at[0] == doctest::Approx(FootstepDirector::kFirstStep).epsilon(0.15));
  for (std::size_t i = 1; i < r.at.size(); ++i)
    CHECK(r.at[i] - r.at[i - 1] == doctest::Approx(FootstepDirector::kStride).epsilon(0.05));
  for (const SoundEvent &e : r.sounds) {
    CHECK(e.set == StepSet::Grass);
    CHECK_FALSE(e.positional);
  }
}

TEST_CASE("sprinting steps come faster, crouching ones are quieter") {
  FootstepDirector walking(1), sprinting(1), crouching(1);
  Walker a = me({0, 64, 0}), b = a, c = a;
  c.crouch = true;
  tick(walking, a);
  tick(sprinting, b);
  tick(crouching, c);
  const auto walked = walk(walking, a, 4.317f, 5.0f).sounds;
  const auto sprinted = walk(sprinting, b, 5.612f, 5.0f).sounds;
  const auto crept = walk(crouching, c, 4.317f, 5.0f).sounds;
  CHECK(sprinted.size() > walked.size());
  REQUIRE(crept.size() == walked.size());
  for (const SoundEvent &e : walked) CHECK(e.volume >= 0.9f);
  for (const SoundEvent &e : crept) {
    CHECK(e.volume >= 0.45f);
    CHECK(e.volume <= 0.55f);
  }
}

TEST_CASE("standing still starts the stride count again") {
  FootstepDirector d(1);
  Walker w = me({0, 64, 0});
  tick(d, w);
  walk(d, w, 4.317f, 0.4f); // a step at 0.5, then 1.2 of the next
  for (int i = 0; i < 30; ++i) tick(d, w);
  Walked r = walk(d, w, 4.317f, 0.5f);
  REQUIRE_FALSE(r.at.empty());
  CHECK(r.at[0] == doctest::Approx(FootstepDirector::kFirstStep).epsilon(0.15));
}

TEST_CASE("no steps flying, swimming, riding or asleep") {
  for (int which = 0; which < 4; ++which) {
    CAPTURE(which);
    FootstepDirector d(1);
    Walker w = me({0, 64, 0});
    w.fly = which == 0;
    w.in_water = which == 1;
    w.riding = which == 2;
    w.sleeping = which == 3;
    tick(d, w);
    CHECK(walk(d, w, 4.317f, 3.0f).sounds.empty());
  }
}

TEST_CASE("silent ground makes no sound") {
  FootstepDirector d(1);
  Walker w = me({0, 64, 0}, StepSet::Silent);
  tick(d, w);
  CHECK(walk(d, w, 4.317f, 3.0f).sounds.empty());
}

TEST_CASE("steps never repeat a recording twice in a row and vary a little") {
  FootstepDirector d(7);
  Walker w = me({0, 64, 0}, StepSet::Dirt);
  tick(d, w);
  const auto sounds = walk(d, w, 5.612f, 120.0f).sounds;
  REQUIRE(sounds.size() > 200);
  std::set<int> used;
  for (std::size_t i = 0; i < sounds.size(); ++i) {
    CHECK(sounds[i].variant >= 0);
    CHECK(sounds[i].variant < kStepVariants[static_cast<std::size_t>(StepSet::Dirt)]);
    if (i) CHECK(sounds[i].variant != sounds[i - 1].variant);
    CHECK(sounds[i].pitch >= 0.94f);
    CHECK(sounds[i].pitch <= 1.06f);
    CHECK(sounds[i].volume >= 0.9f);
    CHECK(sounds[i].volume <= 1.1f);
    used.insert(sounds[i].variant);
  }
  CHECK(used.size() == 10u);
}

namespace {

// Up with the jump speed and down again under gravity, onto `land_y`.
std::vector<SoundEvent> jump(FootstepDirector &d, Walker &w, float land_y, bool into_water = false) {
  std::vector<SoundEvent> out;
  float v = 8.0f;
  w.on_ground = false;
  for (int i = 0; i < 1000; ++i) {
    v -= 28.0f * kDt;
    w.position.y += v * kDt;
    if (v < 0 && w.position.y <= land_y) {
      w.position.y = land_y;
      w.on_ground = !into_water;
      w.in_water = into_water;
      for (const SoundEvent &e : tick(d, w)) out.push_back(e);
      break;
    }
    for (const SoundEvent &e : tick(d, w)) out.push_back(e);
  }
  return out;
}

} // namespace

TEST_CASE("landing from a jump or a fall") {
  SUBCASE("a jump on flat ground is quiet") {
    FootstepDirector d(1);
    Walker w = me({0, 64, 0}, StepSet::Stone);
    tick(d, w);
    const auto sounds = jump(d, w, 64.0f);
    REQUIRE(sounds.size() == 1u);
    CHECK(sounds[0].set == StepSet::Stone);
    CHECK(sounds[0].volume == doctest::Approx(0.6f).epsilon(0.15));
  }
  SUBCASE("a long fall is loud") {
    FootstepDirector d(1);
    Walker w = me({0, 64, 0});
    tick(d, w);
    const auto sounds = jump(d, w, 58.0f);
    REQUIRE(sounds.size() == 1u);
    CHECK(sounds[0].volume >= 0.9f);
  }
  SUBCASE("walking down a slab is not a landing") {
    FootstepDirector d(1);
    Walker w = me({0, 64.5f, 0});
    tick(d, w);
    w.on_ground = false;
    for (int i = 0; i < 10; ++i) {
      w.position.y -= 0.05f;
      CHECK(tick(d, w).empty());
    }
    w.on_ground = true;
    CHECK(tick(d, w).empty());
  }
  SUBCASE("into water is silent") {
    FootstepDirector d(1);
    Walker w = me({0, 64, 0});
    tick(d, w);
    CHECK(jump(d, w, 60.0f, true).empty());
  }
  SUBCASE("others land silently") {
    FootstepDirector d(1);
    Walker w = me({0, 64, 0});
    w.local = false;
    tick(d, w);
    CHECK(jump(d, w, 60.0f).empty());
  }
}

TEST_CASE("ladders: a wood step per block climbed, none hanging still") {
  FootstepDirector d(1);
  Walker w = me({0, 64, 0}, StepSet::Grass);
  w.on_ground = false;
  w.climbing = true;
  tick(d, w);
  int up = 0;
  for (int i = 0; i < 240; ++i) { // 4 s at 2.5 blocks/s: 10 blocks
    w.position.y += 2.5f * kDt;
    for (const SoundEvent &e : tick(d, w)) {
      CHECK(e.set == StepSet::Wood);
      ++up;
    }
  }
  CHECK(up == 10);
  for (int i = 0; i < 300; ++i) CHECK(tick(d, w).empty());
  int down = 0;
  for (int i = 0; i < 200; ++i) { // 1.5 blocks/s for 3.3 s: 5 blocks
    w.position.y -= 1.5f * kDt;
    down += static_cast<int>(tick(d, w).size());
  }
  CHECK(down == 5);
}

TEST_CASE("other players are heard in place, up to 16 blocks away") {
  FootstepDirector d(1);
  Walker w = me({0, 64, 0});
  w.id = 9;
  w.local = false;
  tick(d, w, {0, 64, 0});
  const auto near = walk(d, w, 4.317f, 2.0f, {0, 64, 0}).sounds;
  REQUIRE_FALSE(near.empty());
  for (const SoundEvent &e : near) CHECK(e.positional);

  Walker far = w;
  far.position = {20, 64, 0};
  far.id = 10;
  tick(d, far, {0, 64, 0});
  CHECK(walk(d, far, 4.317f, 3.0f, {0, 64, 0}).sounds.empty());

  // Over air (not on the ground), no steps either.
  Walker over_air = w;
  over_air.id = 11;
  over_air.on_ground = false;
  tick(d, over_air, w.position);
  CHECK(walk(d, over_air, 4.317f, 3.0f, w.position).sounds.empty());
}

TEST_CASE("a walker who leaves and comes back starts fresh") {
  FootstepDirector d(1);
  Walker w = me({0, 64, 0});
  tick(d, w);
  walk(d, w, 4.317f, 0.3f);
  CHECK(d.tracked() == 1u);
  d.update({kDt, {}, {}});
  CHECK(d.tracked() == 0u);
  w.position.x += 30.0f;
  CHECK(tick(d, w).empty());
  Walked r = walk(d, w, 4.317f, 0.5f);
  REQUIRE_FALSE(r.at.empty());
  CHECK(r.at[0] == doctest::Approx(FootstepDirector::kFirstStep).epsilon(0.15));
}

TEST_CASE("a teleport isn't a step or a landing") {
  FootstepDirector d(1);
  Walker w = me({0, 64, 0});
  tick(d, w);
  w.position = {100, 30, 100};
  CHECK(tick(d, w).empty());
}

// ─── Audio ─────────────────────────────────────────────────────────────

#include "audio/audio.hpp"
#include <string>

TEST_CASE("audio: every footstep recording ships and decodes") {
  const EffectPaths paths = footstepPaths();
  std::set<std::string> files;
  for (std::size_t set = 0; set < kStepSetCount; ++set) {
    REQUIRE(paths[set].size() == static_cast<std::size_t>(kStepVariants[set]));
    files.insert(paths[set].begin(), paths[set].end());
  }
  CHECK(files.size() == 10u);
  for (const std::string &p : files) {
    CAPTURE(p);
    CHECK(audioFileReadable(p));
  }
}

TEST_CASE("audio: missing effect files stay silent and don't throw") {
  EffectPaths paths = footstepPaths("no/such/dir");
  Audio audio({"no/such/file.mp3", "no/such/file.mp3"}, paths);
  audio.loadEffects();
  audio.loadEffects(); // once only
  audio.setListener({0, 64, 0}, {0, 0, -1});
  for (int i = 0; i < 40; ++i) {
    SoundEvent e;
    e.set = static_cast<StepSet>(i % kStepSetCount);
    e.variant = i % 5;
    e.positional = i % 2;
    audio.play(e, 1.0f);
  }
  SoundEvent bad;
  bad.variant = 99;
  audio.play(bad, 1.0f);
  CHECK(audio.effectsPlaying() == 0u);
}
