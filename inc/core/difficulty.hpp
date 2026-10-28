#pragma once

#include <algorithm>
#include <optional>
#include <string_view>

// How hard enemies are. Peaceful has none; Easy has fewer, weaker and slower
// ones; Normal uses the values in the enemy table (entity_type.cpp); Hard has
// more of them, tougher and hitting harder.
enum class Difficulty { Peaceful, Easy, Normal, Hard };

constexpr std::string_view difficultyName(Difficulty d) {
  switch (d) {
    case Difficulty::Peaceful: return "peaceful";
    case Difficulty::Easy:     return "easy";
    case Difficulty::Normal:   return "normal";
    case Difficulty::Hard:     return "hard";
  }
  return "easy";
}

constexpr std::optional<Difficulty> parseDifficulty(std::string_view s) {
  if (s == "peaceful") return Difficulty::Peaceful;
  if (s == "easy")     return Difficulty::Easy;
  if (s == "normal")   return Difficulty::Normal;
  if (s == "hard")     return Difficulty::Hard;
  return std::nullopt;
}

// Easy halves caps and damage (never below 1) and slows enemies to 80%.
// Peaceful has no enemies, so it scales like Easy for anything that asks.
// Hard makes caps, damage and health 1.5 times Normal's, rounded up, and
// tries to spawn a third more often.
constexpr int harder(int n) { return (n * 3 + 1) / 2; }

constexpr int scaledCap(int cap, Difficulty d) {
  if (d == Difficulty::Hard) return harder(cap);
  return d == Difficulty::Normal ? cap : std::max(1, cap / 2);
}
constexpr int scaledDamage(int damage, Difficulty d) {
  if (d == Difficulty::Hard) return harder(damage);
  return d == Difficulty::Normal ? damage : std::max(1, damage / 2);
}
constexpr int scaledHealth(int health, Difficulty d) {
  return d == Difficulty::Hard ? harder(health) : health;
}
constexpr int scaledInterval(int interval_ms, Difficulty d) {
  return d == Difficulty::Hard ? interval_ms * 3 / 4 : interval_ms;
}
constexpr float speedScale(Difficulty d) {
  return d == Difficulty::Normal || d == Difficulty::Hard ? 1.0f : 0.8f;
}

// Normal and Hard: health only comes back while well fed.
constexpr bool needsFood(Difficulty d) {
  return d == Difficulty::Normal || d == Difficulty::Hard;
}
