#include "item/tools.hpp"
#include <array>

namespace {

constexpr int kTiers = 7; // Wood..Diamond
constexpr uint16_t kFirstTool = static_cast<uint16_t>(ItemId::WOOD_PICKAXE);

constexpr std::array<ToolKind, 4> kKinds = {ToolKind::Pickaxe, ToolKind::Axe, ToolKind::Shovel,
                                            ToolKind::Sword};

} // namespace

Tool toolOf(ItemId id) {
  auto v = static_cast<uint16_t>(id);
  if (v < kFirstTool || v > static_cast<uint16_t>(ItemId::DIAMOND_SWORD)) return {};
  int i = v - kFirstTool;
  return {kKinds[i / kTiers], static_cast<Tier>(1 + i % kTiers)};
}

ItemId toolItem(ToolKind kind, Tier tier) {
  if (kind == ToolKind::None || tier == Tier::Hand) return ItemId::NONE;
  int k = static_cast<int>(kind) - 1;
  int t = static_cast<int>(tier) - 1;
  return static_cast<ItemId>(kFirstTool + k * kTiers + t);
}

int miningSpeed(Tier tier) {
  constexpr std::array<int, 8> speed = {1, 2, 4, 6, 8, 10, 10, 12};
  return speed[static_cast<int>(tier)];
}

int maxUses(Tier tier) {
  constexpr std::array<int, 8> uses = {0, 60, 130, 200, 250, 1000, 1000, 1600};
  return uses[static_cast<int>(tier)];
}

int usesOf(ItemId id) {
  if (id == ItemId::FISHING_ROD) return 64;
  Tool t = toolOf(id);
  return t.kind == ToolKind::None ? 0 : maxUses(t.tier);
}

int swordDamage(Tier tier) {
  constexpr std::array<int, 8> damage = {1, 3, 4, 5, 6, 7, 7, 8};
  return damage[static_cast<int>(tier)];
}

int attackDamage(ItemId held) {
  Tool t = toolOf(held);
  switch (t.kind) {
    case ToolKind::Sword:   return swordDamage(t.tier);
    case ToolKind::Axe:     return swordDamage(t.tier) - 1;
    case ToolKind::Pickaxe:
    case ToolKind::Shovel:  return 2;
    default:                return 1;
  }
}
