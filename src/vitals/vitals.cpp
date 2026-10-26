#include "vitals/vitals.hpp"
#include <algorithm>
#include <cmath>

// ─── Vitals ────────────────────────────────────────────────────────────

void Vitals::tick(int dt_ms, const Activity &a) {
  if (dead() || dt_ms <= 0) return;

  // Stamina: sprinting drains it; after a short rest it recovers.
  if (a.sprinting && !a.fly) {
    stamina_rest = 0;
    stamina_acc += dt_ms;
    while (stamina_acc >= kSprintDrainMs && stamina_ > 0) {
      stamina_acc -= kSprintDrainMs;
      --stamina_;
    }
    if (stamina_ == 0) {
      exhausted = true;
      stamina_acc = 0;
    }
  } else {
    if (stamina_rest < kStaminaRestMs) {
      stamina_rest += dt_ms;
      stamina_acc = 0;
    } else {
      stamina_acc += dt_ms;
      while (stamina_acc >= kStaminaRegenMs && stamina_ < kMax) {
        stamina_acc -= kStaminaRegenMs;
        ++stamina_;
      }
      if (stamina_ == kMax) stamina_acc = 0;
    }
  }
  if (exhausted && stamina_ >= kSprintResume) exhausted = false;

  // Breath: drains with eyes underwater, then drowning hurts every second.
  if (a.eyes_underwater) {
    if (breath_ > 0) {
      breath_acc += dt_ms;
      while (breath_acc >= kBreathDrainMs && breath_ > 0) {
        breath_acc -= kBreathDrainMs;
        --breath_;
      }
      if (breath_ == 0) breath_acc = drown_acc = 0;
    } else {
      drown_acc += dt_ms;
      while (drown_acc >= kDrownIntervalMs && !dead()) {
        drown_acc -= kDrownIntervalMs;
        damage(kDrownDamage);
      }
    }
  } else {
    drown_acc = 0;
    breath_acc += dt_ms;
    while (breath_acc >= kBreathRefillMs && breath_ < kMax) {
      breath_acc -= kBreathRefillMs;
      ++breath_;
    }
    if (breath_ == kMax) breath_acc = 0;
  }
  if (dead()) return;

  // Health: regenerates once a while has passed without damage.
  if (since_damage < kRegenDelayMs) {
    since_damage += dt_ms;
  } else if (health_ < kMax) {
    regen_acc += dt_ms;
    while (regen_acc >= kRegenMs && health_ < kMax) {
      regen_acc -= kRegenMs;
      ++health_;
    }
  }
}

void Vitals::damage(int amount) {
  if (dead() || amount <= 0) return;
  health_ = std::max(0, health_ - amount);
  since_damage = 0;
  regen_acc = 0;
}

void Vitals::reset() { *this = Vitals{}; }

// ─── FallTracker ───────────────────────────────────────────────────────

int FallTracker::update(float y, bool on_ground, bool in_water, bool fly) {
  if (fly || in_water) {
    airborne = false;
    have_ground = true;
    ground_y = y;
    return 0;
  }
  if (!on_ground) {
    if (!airborne) {
      airborne = true;
      peak = have_ground ? std::max(ground_y, y) : y;
    } else {
      peak = std::max(peak, y);
    }
    return 0;
  }

  int dmg = 0;
  if (airborne) {
    float fall = peak - y;
    // A hair of slack so an exact 4-block drop counts as 4, not 3.9999.
    dmg = std::max(0, static_cast<int>(std::floor(fall - kSafeFall + 1e-3f)));
  }
  airborne = false;
  have_ground = true;
  ground_y = y;
  return dmg;
}
