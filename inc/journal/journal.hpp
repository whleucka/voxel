#pragma once

#include "entity/entity_type.hpp"
#include "item/item.hpp"
#include "item/recipes.hpp" // SeenItems
#include <bitset>
#include <cstdint>
#include <string_view>
#include <variant>
#include <vector>

// The Journal: a player's own record of what they've done and found. Pure
// data and logic; the Game records it and the HUD draws it.
//
// Item progress comes from SeenItems (every item a player has ever had),
// which is already saved and already sent to the client. The journal adds
// only the deeds below, the creature kinds met, and (later) chain stamps.

// Things a player has done at least once. Values are saved as bits: append only.
enum class Deed : uint8_t { Slept, FedAnimal, TamedPet, Sailed, Tilled, OpenedTreasure };
constexpr size_t kDeedCount = 6;

using Deeds = std::bitset<16>;
using Met = std::bitset<32>;     // by EntityKind value
using Stamps = std::bitset<16>;  // by chain index

struct JournalProgress {
  Deeds deeds;
  Met met;
  Stamps stamps;
  bool operator==(const JournalProgress &) const = default;

  bool did(Deed d) const { return deeds.test(static_cast<size_t>(d)); }
  void set(Deed d) { deeds.set(static_cast<size_t>(d)); }
  bool hasMet(EntityKind k) const;
  void meet(EntityKind k);
};

// The creature kinds the Creatures page lists: met by being near a living one.
bool isJournalCreature(EntityKind k);

// Collections.
enum class Page : uint8_t { Creatures, Fish, GemsOres, Plants };
constexpr int kPageCount = 4;
std::string_view pageTitle(Page p);

struct Entry {
  Page page;
  std::variant<ItemId, EntityKind> what;
  std::string_view hint;  // where to look: shown even before it's found
  int portrait = -1;      // creatures: tile in journal.png (row 0)
};
const std::vector<Entry> &entries();

std::string_view entryName(const Entry &e);
bool entryFilled(const Entry &e, const SeenItems &seen, const JournalProgress &j);

struct PageCount { int filled = 0; int total = 0; };
PageCount pageCount(Page p, const SeenItems &seen, const JournalProgress &j);

// Entries filled in `now` that weren't in `before` (same progress for both,
// save for the seen items), in entries() order: for "New in your journal".
std::vector<const Entry *> newlyFilled(const SeenItems &before, const JournalProgress &j_before,
                                       const SeenItems &now, const JournalProgress &j_now);
