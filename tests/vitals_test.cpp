#include "doctest/doctest.h"
#include "vitals/vitals.hpp"

namespace {

constexpr int kTick = 50; // the Game's tick, in ms

const Activity kIdle{};
const Activity kSprint{true, false, false};
const Activity kUnderwater{false, true, false};

void run(Vitals &v, int ms, const Activity &a) {
  for (int t = 0; t < ms; t += kTick) v.tick(kTick, a);
}

} // namespace

TEST_CASE("everything starts full") {
  Vitals v;
  CHECK(v.health() == 20);
  CHECK(v.stamina() == 20);
  CHECK(v.breath() == 20);
  CHECK(v.canSprint());
  CHECK_FALSE(v.dead());
}

// ─── Stamina ───────────────────────────────────────────────────────────

TEST_CASE("sprinting drains 2 stamina per second") {
  Vitals v;
  run(v, 1000, kSprint);
  CHECK(v.stamina() == 18);
  run(v, 4000, kSprint);
  CHECK(v.stamina() == 10);
}

TEST_CASE("10 s of sprinting exhausts; sprinting resumes at 6") {
  Vitals v;
  run(v, 10000, kSprint);
  CHECK(v.stamina() == 0);
  CHECK_FALSE(v.canSprint());

  run(v, 1000, kIdle); // rest period: no recovery yet
  CHECK(v.stamina() == 0);
  run(v, 1000, kIdle); // 4 / s
  CHECK(v.stamina() == 4);
  CHECK_FALSE(v.canSprint());
  run(v, 500, kIdle);
  CHECK(v.stamina() == 6);
  CHECK(v.canSprint());
}

TEST_CASE("stamina recovers fully after a rest and never overshoots") {
  Vitals v;
  run(v, 2000, kSprint); // 16
  run(v, 1000 + 1000, kIdle);
  CHECK(v.stamina() == 20);
  run(v, 5000, kIdle);
  CHECK(v.stamina() == 20);
}

TEST_CASE("sprinting again restarts the rest period") {
  Vitals v;
  run(v, 2000, kSprint); // 16
  run(v, 950, kIdle);
  run(v, 50, kSprint);
  run(v, 950, kIdle);
  CHECK(v.stamina() == 16);
}

TEST_CASE("fly mode doesn't drain stamina") {
  Vitals v;
  run(v, 5000, Activity{true, false, true});
  CHECK(v.stamina() == 20);
}

// ─── Breath and drowning ───────────────────────────────────────────────

TEST_CASE("breath lasts 10 s underwater, then drowning costs a heart a second") {
  Vitals v;
  run(v, 5000, kUnderwater);
  CHECK(v.breath() == 10);
  CHECK(v.health() == 20);
  run(v, 5000, kUnderwater);
  CHECK(v.breath() == 0);
  CHECK(v.health() == 20);
  run(v, 1000, kUnderwater);
  CHECK(v.health() == 18);
  run(v, 2000, kUnderwater);
  CHECK(v.health() == 14);
}

TEST_CASE("breath refills quickly above water") {
  Vitals v;
  run(v, 6000, kUnderwater); // 8
  run(v, 1000, kIdle);
  CHECK(v.breath() == 18);
  run(v, 500, kIdle);
  CHECK(v.breath() == 20);
}

TEST_CASE("staying underwater long enough is fatal") {
  Vitals v;
  run(v, 10000 + 10000, kUnderwater);
  CHECK(v.dead());
  CHECK(v.health() == 0);
}

// ─── Health ────────────────────────────────────────────────────────────

TEST_CASE("health regenerates after a 4 s delay, 1 point per 2 s") {
  Vitals v;
  v.damage(5);
  CHECK(v.health() == 15);
  run(v, 4000, kIdle);
  CHECK(v.health() == 15);
  run(v, 2000, kIdle);
  CHECK(v.health() == 16);
  run(v, 8000, kIdle);
  CHECK(v.health() == 20);
  run(v, 4000, kIdle);
  CHECK(v.health() == 20);
}

TEST_CASE("damage restarts the regen delay") {
  Vitals v;
  v.damage(4);
  run(v, 5000, kIdle); // regen running, nothing gained yet
  v.damage(1);
  run(v, 4000 + 1950, kIdle);
  CHECK(v.health() == 15);
  run(v, 50, kIdle);
  CHECK(v.health() == 16);
}

TEST_CASE("health clamps at 0; the dead stay dead until reset") {
  Vitals v;
  v.damage(50);
  CHECK(v.health() == 0);
  CHECK(v.dead());
  run(v, 20000, kIdle);
  CHECK(v.health() == 0);
  v.damage(3);
  CHECK(v.health() == 0);

  v.reset();
  CHECK(v.health() == 20);
  CHECK(v.stamina() == 20);
  CHECK(v.breath() == 20);
  CHECK_FALSE(v.dead());
}

TEST_CASE("zero or negative damage does nothing") {
  Vitals v;
  v.damage(0);
  v.damage(-3);
  CHECK(v.health() == 20);
}

// ─── Falls ─────────────────────────────────────────────────────────────

TEST_CASE("falls of 3 blocks or less are safe; 4 blocks costs 1") {
  FallTracker f;
  f.update(70.0f, true, false, false);
  f.update(69.9f, false, false, false);
  CHECK(f.update(67.0f, true, false, false) == 0); // 3 blocks

  f.update(66.9f, false, false, false);
  CHECK(f.update(63.0f, true, false, false) == 1); // 4 blocks from the ledge

  f.update(62.9f, false, false, false);
  CHECK(f.update(53.0f, true, false, false) == 7); // 10 blocks
}

TEST_CASE("a fall is measured from the peak of a jump") {
  FallTracker f;
  f.update(70.0f, true, false, false);
  f.update(71.0f, false, false, false);
  f.update(71.25f, false, false, false); // apex
  f.update(70.5f, false, false, false);
  CHECK(f.update(70.0f, true, false, false) == 0); // a plain jump

  f.update(71.25f, false, false, false);
  f.update(68.0f, false, false, false);
  CHECK(f.update(66.0f, true, false, false) == 2); // 5.25 from the apex
}

TEST_CASE("landing in water or flying is safe") {
  FallTracker f;
  f.update(80.0f, true, false, false);
  f.update(79.0f, false, false, false);
  CHECK(f.update(60.0f, false, true, false) == 0); // splash
  CHECK(f.update(60.0f, true, false, false) == 0); // climb out at the bottom

  f.update(90.0f, false, false, true);             // flying high
  f.update(89.0f, false, false, false);            // fly switched off
  CHECK(f.update(87.0f, true, false, false) == 0); // 3 from where flying ended
}

TEST_CASE("each landing counts on its own; reset forgets the fall") {
  FallTracker f;
  f.update(70.0f, true, false, false);
  f.update(69.0f, false, false, false);
  CHECK(f.update(60.0f, true, false, false) == 7);
  CHECK(f.update(60.0f, true, false, false) == 0);

  f.update(59.0f, false, false, false);
  f.reset(); // e.g. respawned mid-fall
  f.update(40.0f, false, false, false);
  CHECK(f.update(39.0f, true, false, false) == 0);
}

TEST_CASE("heal restores health up to the max and not for the dead") {
  Vitals v;
  v.damage(10);
  v.heal(4);
  CHECK(v.health() == 14);
  v.heal(50);
  CHECK(v.health() == 20);
  v.damage(30);
  v.heal(5);
  CHECK(v.health() == 0);
}
