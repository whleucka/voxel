#include "entity/player_look.hpp"
#include <array>

namespace {
constexpr std::array<LookSwatch, 1 + kPlayerColors + 2> kShirts = {{
    {"auto", {0.50f, 0.50f, 0.50f}},
    {"red", {0.90f, 0.25f, 0.25f}},
    {"blue", {0.25f, 0.50f, 0.95f}},
    {"green", {0.30f, 0.80f, 0.35f}},
    {"yellow", {0.98f, 0.80f, 0.20f}},
    {"purple", {0.70f, 0.35f, 0.90f}},
    {"orange", {0.98f, 0.55f, 0.15f}},
    {"teal", {0.20f, 0.80f, 0.80f}},
    {"pink", {0.95f, 0.45f, 0.70f}},
    {"white", {0.92f, 0.92f, 0.90f}},
    {"black", {0.12f, 0.12f, 0.14f}},
}};
constexpr std::array<LookSwatch, 8> kHair = {{
    {"brown", {0.30f, 0.20f, 0.12f}},
    {"black", {0.08f, 0.07f, 0.07f}},
    {"blond", {0.88f, 0.72f, 0.36f}},
    {"ginger", {0.72f, 0.33f, 0.12f}},
    {"grey", {0.55f, 0.55f, 0.56f}},
    {"white", {0.90f, 0.90f, 0.88f}},
    {"blue", {0.20f, 0.40f, 0.90f}},
    {"pink", {0.95f, 0.45f, 0.70f}},
}};
constexpr std::array<LookSwatch, 6> kSkin = {{
    {"pale", {0.98f, 0.85f, 0.74f}},
    {"fair", {0.93f, 0.76f, 0.60f}},
    {"tan", {0.82f, 0.62f, 0.44f}},
    {"olive", {0.70f, 0.52f, 0.34f}},
    {"brown", {0.50f, 0.34f, 0.22f}},
    {"dark", {0.32f, 0.21f, 0.14f}},
}};
constexpr std::array<LookSwatch, 6> kEyes = {{
    {"dark", {0.08f, 0.08f, 0.10f}},
    {"brown", {0.32f, 0.18f, 0.08f}},
    {"blue", {0.15f, 0.40f, 0.85f}},
    {"green", {0.15f, 0.60f, 0.25f}},
    {"grey", {0.45f, 0.50f, 0.55f}},
    {"purple", {0.50f, 0.20f, 0.75f}},
}};

uint8_t inRange(uint8_t value, size_t count, uint8_t fallback) {
  return value < count ? value : fallback;
}
} // namespace

std::span<const LookSwatch> lookSwatches(LookPart part) {
  switch (part) {
    case LookPart::Shirt: return kShirts;
    case LookPart::Hair: return kHair;
    case LookPart::Skin: return kSkin;
    case LookPart::Eyes: return kEyes;
  }
  return {};
}

uint8_t &lookField(PlayerLook &look, LookPart part) {
  switch (part) {
    case LookPart::Shirt: return look.shirt;
    case LookPart::Hair: return look.hair;
    case LookPart::Skin: return look.skin;
    case LookPart::Eyes: return look.eyes;
  }
  return look.shirt;
}

std::optional<uint8_t> parseFigure(std::string_view name) {
  for (uint8_t i = 0; i < std::size(kFigureNames); ++i)
    if (name == kFigureNames[i]) return i;
  return std::nullopt;
}

std::optional<uint8_t> parseSwatch(LookPart part, std::string_view name) {
  std::span<const LookSwatch> swatches = lookSwatches(part);
  for (size_t i = 0; i < swatches.size(); ++i)
    if (name == swatches[i].name) return static_cast<uint8_t>(i);
  return std::nullopt;
}

PlayerLook sanitized(PlayerLook look) {
  const PlayerLook d;
  look.figure = inRange(look.figure, std::size(kFigureNames), d.figure);
  look.shirt = inRange(look.shirt, kShirts.size(), d.shirt);
  look.hair = inRange(look.hair, kHair.size(), d.hair);
  look.skin = inRange(look.skin, kSkin.size(), d.skin);
  look.eyes = inRange(look.eyes, kEyes.size(), d.eyes);
  return look;
}

const glm::vec3 &playerColor(std::string_view name) {
  // FNV-1a: stable across runs and machines.
  uint32_t h = 2166136261u;
  for (char c : name) h = (h ^ static_cast<uint8_t>(c)) * 16777619u;
  return kShirts[1 + h % kPlayerColors].color;
}

ResolvedLook resolveLook(PlayerLook look, std::string_view name) {
  look = sanitized(look);
  return {look.figure == kFigureGirl,
          look.shirt == 0 ? playerColor(name) : kShirts[look.shirt].color,
          kHair[look.hair].color, kSkin[look.skin].color, kEyes[look.eyes].color};
}
