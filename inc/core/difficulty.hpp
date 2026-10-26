#pragma once

#include <algorithm>
#include <optional>
#include <string_view>

// How hard enemies are. Peaceful has none; Easy has fewer, weaker and slower
// ones; Normal uses the values in the enemy table (SPEC-enemies.md).
enum class Difficulty { Peaceful, Easy, Normal };

constexpr std::string_view difficultyName(Difficulty d) {
  switch (d) {
    case Difficulty::Peaceful: return "peaceful";
    case Difficulty::Easy:     return "easy";
    case Difficulty::Normal:   return "normal";
  }
  return "easy";
}

constexpr std::optional<Difficulty> parseDifficulty(std::string_view s) {
  if (s == "peaceful") return Difficulty::Peaceful;
  if (s == "easy")     return Difficulty::Easy;
  if (s == "normal")   return Difficulty::Normal;
  return std::nullopt;
}

// Easy halves caps and damage (never below 1) and slows enemies to 80%.
// Peaceful has no enemies, so it scales like Easy for anything that asks.
constexpr int scaledCap(int cap, Difficulty d) {
  return d == Difficulty::Normal ? cap : std::max(1, cap / 2);
}
constexpr int scaledDamage(int damage, Difficulty d) {
  return d == Difficulty::Normal ? damage : std::max(1, damage / 2);
}
constexpr float speedScale(Difficulty d) {
  return d == Difficulty::Normal ? 1.0f : 0.8f;
}
