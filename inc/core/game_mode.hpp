#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

// A world's game mode, saved in its level.dat. Every difference between the
// modes is in ModeRules, so code asks about a rule rather than the mode.
enum class GameMode : uint8_t { Survival, Creative };

constexpr std::string_view gameModeName(GameMode m) {
  return m == GameMode::Creative ? "creative" : "survival";
}

constexpr std::optional<GameMode> parseGameMode(std::string_view s) {
  if (s == "survival") return GameMode::Survival;
  if (s == "creative") return GameMode::Creative;
  return std::nullopt;
}

struct ModeRules {
  bool fly;            // T and the debug checkbox may turn flying on
  bool consume_blocks; // placing uses one from the held stack
  bool block_drops;    // breaking collects the block
  bool damage;         // enemies, falls and drowning hurt
  bool drain;          // stamina and breath run down
  bool enemies_target; // enemies chase and hit the player
  bool palette;        // the inventory offers every item (anyone may GiveItem)
};

constexpr ModeRules rulesFor(GameMode m) {
  if (m == GameMode::Creative) return {true, false, false, false, false, false, true};
  return {false, true, true, true, true, true, false};
}
