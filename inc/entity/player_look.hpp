#pragma once

#include <glm/vec3.hpp>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

// What a player chose on the Character page. Every value indexes a list below,
// and the defaults look like the figure did before there was a choice.
constexpr uint8_t kDefaultSkin = 1;
struct PlayerLook {
  uint8_t figure = 0; // kFigureNames
  uint8_t shirt = 0;  // 0: Auto, the colour from the name
  uint8_t hair = 0;
  uint8_t skin = kDefaultSkin;
  uint8_t eyes = 0;
  bool operator==(const PlayerLook &) const = default;
};

enum class LookPart { Shirt, Hair, Skin, Eyes };
struct LookSwatch {
  const char *name; // also its value in voxel.properties
  glm::vec3 color;  // sRGB; Auto's is unused
};
std::span<const LookSwatch> lookSwatches(LookPart part);
uint8_t &lookField(PlayerLook &look, LookPart part);

constexpr uint8_t kFigureGirl = 1;
constexpr const char *kFigureNames[] = {"boy", "girl"};
std::optional<uint8_t> parseFigure(std::string_view name);
std::optional<uint8_t> parseSwatch(LookPart part, std::string_view name);

// Anything out of range (a bad save or a bad peer) back to its default.
PlayerLook sanitized(PlayerLook look);

// The bright, distinct shirts Auto chooses from.
constexpr int kPlayerColors = 8;
const glm::vec3 &playerColor(std::string_view name);

// A look's actual colours, safe to call with any values.
struct ResolvedLook {
  bool girl = false;
  glm::vec3 shirt, hair, skin, eyes;
};
ResolvedLook resolveLook(PlayerLook look, std::string_view name);
