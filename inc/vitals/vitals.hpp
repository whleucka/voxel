#pragma once

#include <cstdint>

// What the player was doing during a tick, as reported by their client.
struct Activity {
  bool sprinting = false;
  bool eyes_underwater = false;
  bool fly = false;
};

// Health, stamina and breath for one player, in half-icon points (0..20).
// Pure logic driven by the Game's tick. Rates are integer "milliseconds per
// point" accumulated from whole-millisecond ticks, so values change at exact,
// testable times.
class Vitals {
public:
  static constexpr int kMax = 20;

  // Stamina
  static constexpr int kSprintDrainMs = 500;   // 2 points / s while sprinting
  static constexpr int kStaminaRestMs = 1000;  // pause before recovering
  static constexpr int kStaminaRegenMs = 250;  // 4 points / s
  static constexpr int kSprintResume = 6;      // exhausted until back to this
  // Breath
  static constexpr int kBreathDrainMs = 500;   // 2 points / s: 10 s underwater
  static constexpr int kBreathRefillMs = 100;  // 10 points / s
  static constexpr int kDrownIntervalMs = 1000;
  static constexpr int kDrownDamage = 2;       // one heart per second
  // Health
  static constexpr int kRegenDelayMs = 4000;   // since the last damage
  static constexpr int kRegenMs = 2000;        // 1 point every 2 s

  int health() const { return health_; }
  int stamina() const { return stamina_; }
  int breath() const { return breath_; }
  bool dead() const { return health_ <= 0; }
  bool canSprint() const { return !exhausted && stamina_ > 0; }

  void tick(int dt_ms, const Activity &a);
  void damage(int amount); // resets the regen delay; ignored once dead
  void heal(int amount);   // up to kMax; ignored once dead
  void reset();            // full everything (respawn)

private:
  int health_ = kMax;
  int stamina_ = kMax;
  int breath_ = kMax;
  bool exhausted = false;

  int stamina_acc = 0;  // ms toward the next stamina change
  int stamina_rest = 0; // ms since sprinting stopped
  int breath_acc = 0;
  int drown_acc = 0;
  int since_damage = kRegenDelayMs;
  int regen_acc = 0;
};

// Fall damage from the client's reported positions: the drop from the highest
// point of an airborne stretch (or the ground it left) to where it lands.
class FallTracker {
public:
  static constexpr float kSafeFall = 3.0f; // blocks

  // Feed every reported position. Returns the damage on landing, else 0.
  // Water and fly mode are safe and restart the measurement.
  int update(float y, bool on_ground, bool in_water, bool fly);
  void reset() { airborne = false; have_ground = false; }

private:
  bool airborne = false;
  bool have_ground = false;
  float peak = 0.0f;     // highest point since leaving the ground
  float ground_y = 0.0f; // last height stood on (or swum/flown at)
};
