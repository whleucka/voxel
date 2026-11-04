#pragma once

#include "entity/entity_type.hpp"
#include "item/item.hpp"
#include "item/item_stack.hpp"
#include <optional>
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

struct JournalState;
JournalState toWire(const JournalProgress &j);
JournalProgress fromWire(const JournalState &s);

// The creature kinds the Creatures page lists: met by being near a living one.
bool isJournalCreature(EntityKind k);

// Collections.
enum class JournalPage : uint8_t { Creatures, Fish, GemsOres, Plants };
constexpr int kPageCount = 4;
std::string_view pageTitle(JournalPage p);
// "Pat found every <this>!": creature, fish, gem and ore, plant.
std::string_view pageEvery(JournalPage p);

struct Entry {
  JournalPage page;
  std::variant<ItemId, EntityKind> what;
  std::string_view hint;  // where to look: shown even before it's found
  int portrait = -1;      // creatures: tile in journal.png (row 0)
  std::string_view name = {}; // instead of the item's ("Glowfin", not "Raw Glowfin")
};
const std::vector<Entry> &entries();

std::string_view entryName(const Entry &e);
bool entryFilled(const Entry &e, const SeenItems &seen, const JournalProgress &j);

struct PageCount { int filled = 0; int total = 0; };
PageCount pageCount(JournalPage p, const SeenItems &seen, const JournalProgress &j);

// Entries filled in `now` that weren't in `before` (same progress for both,
// save for the seen items), in entries() order: for "New in your journal".
std::vector<const Entry *> newlyFilled(const SeenItems &before, const JournalProgress &j_before,
                                       const SeenItems &now, const JournalProgress &j_now);

// Notes: chains of small goals, in the player's own words. A step is done
// when any of its items has been seen or its deed done. Steps show in order:
// the done ones, then the next. Finishing a chain earns its stamp and gift.
struct Step {
  std::vector<ItemId> any_of;
  std::optional<Deed> deed;
  std::string_view hint; // before: what to try
  std::string_view done; // after
};

struct Chain {
  std::string_view title;
  std::vector<Step> steps;
  ItemStack gift;
  std::string_view gift_text; // "8 Torches"
  bool starts_open = false; // the rest open once the first chain's first step is done
};
// A chain's index is its stamp bit: append only.
const std::vector<Chain> &chains();

bool stepDone(const Step &step, const SeenItems &seen, const JournalProgress &j);
// How many steps, from the first, are done: the next one is the one to show.
int stepsDone(const Chain &chain, const SeenItems &seen, const JournalProgress &j);
bool chainOpen(int index, const SeenItems &seen, const JournalProgress &j);
bool chainComplete(const Chain &chain, const SeenItems &seen, const JournalProgress &j);
// Complete chains without their stamp yet, in order.
std::vector<int> chainsToStamp(const SeenItems &seen, const JournalProgress &j);
