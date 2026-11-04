#include "journal/journal.hpp"
#include "block/bed.hpp"
#include "block/block_type.hpp"
#include "sim/protocol.hpp"

bool JournalProgress::hasMet(EntityKind k) const {
  const auto v = static_cast<size_t>(k);
  return v < met.size() && met.test(v);
}

void JournalProgress::meet(EntityKind k) {
  const auto v = static_cast<size_t>(k);
  if (v < met.size()) met.set(v);
}

JournalState toWire(const JournalProgress &j) {
  return {static_cast<uint16_t>(j.deeds.to_ulong()), static_cast<uint32_t>(j.met.to_ulong()),
          static_cast<uint16_t>(j.stamps.to_ulong())};
}

JournalProgress fromWire(const JournalState &s) {
  return {Deeds(s.deeds), Met(s.met), Stamps(s.stamps)};
}

bool isJournalCreature(EntityKind k) {
  switch (k) {
    case EntityKind::Woolly: case EntityKind::Bunny: case EntityKind::ShellTurtle:
    case EntityKind::BlossomDeer: case EntityKind::Puppy: case EntityKind::Kitten:
    case EntityKind::Gloomling: case EntityKind::ThornCrawler: case EntityKind::Zombie:
    case EntityKind::Mummy: case EntityKind::CaveGrumble:
      return true;
    default:
      return false;
  }
}

std::string_view pageTitle(JournalPage p) {
  switch (p) {
    case JournalPage::Creatures: return "Creatures";
    case JournalPage::Fish:      return "Fish";
    case JournalPage::GemsOres:  return "Gems & Ores";
    case JournalPage::Plants:    return "Plants";
  }
  return "";
}

std::string_view pageEvery(JournalPage p) {
  switch (p) {
    case JournalPage::Creatures: return "creature";
    case JournalPage::Fish:      return "fish";
    case JournalPage::GemsOres:  return "gem and ore";
    case JournalPage::Plants:    return "plant";
  }
  return "";
}

const std::vector<Entry> &entries() {
  using enum JournalPage;
  auto block = [](BlockType b) { return itemForBlock(b); };
  static const std::vector<Entry> list = {
      {Creatures, EntityKind::Woolly, "Mountains and plains. Loves leaves.", 0},
      {Creatures, EntityKind::Bunny, "Plains and cherry groves. Hop hop!", 1},
      {Creatures, EntityKind::ShellTurtle, "Tropical beaches. Likes palm leaves.", 2},
      {Creatures, EntityKind::BlossomDeer, "Cherry groves, among the pink trees.", 3},
      {Creatures, EntityKind::Puppy, "A stray on the plains or mountains.", 4},
      {Creatures, EntityKind::Kitten, "A stray on plains, cherry groves or sand.", 5},
      {Creatures, EntityKind::Gloomling, "Hops about at night. Careful!", 6},
      {Creatures, EntityKind::ThornCrawler, "Creeps through the grass at night.", 7},
      {Creatures, EntityKind::Zombie, "Shuffles about at night.", 8},
      {Creatures, EntityKind::Mummy, "Wanders the desert, even by day.", 9},
      {Creatures, EntityKind::CaveGrumble, "Grumbles deep in dark caves.", 10},

      {Fish, ItemId::RAW_PEBBLEFISH, "Lakes and rivers. Catch one with a rod.", -1, "Pebblefish"},
      {Fish, ItemId::RAW_SUNFIN, "Out in the sea.", -1, "Sunfin"},
      {Fish, ItemId::RAW_GLOWFIN, "Cave pools. It glows!", -1, "Glowfin"},
      {Fish, ItemId::SOGGY_BOOT, "Sometimes the line catches... this.", -1},

      {GemsOres, ItemId::COAL, "Black specks in stone. Needs a stone pick.", -1},
      {GemsOres, ItemId::RAW_COPPER, "Orange specks in stone. Needs a stone pick.", -1},
      {GemsOres, ItemId::RAW_IRON, "Specks in stone. Needs a copper pick.", -1},
      {GemsOres, ItemId::RAW_GOLD, "Deeper down. Needs a copper pick.", -1},
      {GemsOres, ItemId::EMERALD, "Deep down. Needs an iron pick.", -1},
      {GemsOres, ItemId::RUBY, "Deep down. Red, and needs an iron pick.", -1},
      {GemsOres, ItemId::DIAMOND, "The deepest gem of all. Iron pick!", -1},

      {Plants, block(BlockType::POPPY), "Plains meadows.", -1},
      {Plants, block(BlockType::BUTTERCUP), "Plains meadows.", -1},
      {Plants, block(BlockType::CORNFLOWER), "Plains meadows.", -1},
      {Plants, block(BlockType::PINK_TULIP), "Cherry groves.", -1},
      {Plants, block(BlockType::DAISY), "Cherry groves.", -1},
      {Plants, block(BlockType::LAVENDER), "Up in the mountains.", -1},
      {Plants, block(BlockType::HIBISCUS), "Tropical islands.", -1},
      {Plants, block(BlockType::FERN), "Mountain slopes.", -1},
      {Plants, block(BlockType::RED_MUSHROOM), "Under mountain pines.", -1},
      {Plants, block(BlockType::BROWN_MUSHROOM), "Under pines, and in caves.", -1},
      {Plants, block(BlockType::BERRY_BUSH), "Tropical islands.", -1},
      {Plants, block(BlockType::CACTUS), "The desert. Ouch!", -1},
      {Plants, block(BlockType::PINK_CORAL), "Under warm seas.", -1},
      {Plants, block(BlockType::BLUE_CORAL), "Under warm seas.", -1},
      {Plants, block(BlockType::YELLOW_CORAL), "Under warm seas.", -1},
      {Plants, ItemId::SEA_SHELL, "On the beach.", -1},
      {Plants, ItemId::SWEET_PETALS, "A Blossom Deer's thank-you.", -1},
  };
  return list;
}

std::string_view entryName(const Entry &e) {
  if (!e.name.empty()) return e.name;
  if (const auto *k = std::get_if<EntityKind>(&e.what)) return entityType(*k).name;
  return itemDef(std::get<ItemId>(e.what)).name;
}

bool entryFilled(const Entry &e, const SeenItems &seen, const JournalProgress &j) {
  if (const auto *k = std::get_if<EntityKind>(&e.what)) return j.hasMet(*k);
  const auto v = static_cast<size_t>(std::get<ItemId>(e.what));
  return v < seen.size() && seen.test(v);
}

PageCount pageCount(JournalPage p, const SeenItems &seen, const JournalProgress &j) {
  PageCount c;
  for (const Entry &e : entries()) {
    if (e.page != p) continue;
    ++c.total;
    if (entryFilled(e, seen, j)) ++c.filled;
  }
  return c;
}

std::vector<const Entry *> newlyFilled(const SeenItems &before, const JournalProgress &j_before,
                                       const SeenItems &now, const JournalProgress &j_now) {
  std::vector<const Entry *> out;
  for (const Entry &e : entries())
    if (entryFilled(e, now, j_now) && !entryFilled(e, before, j_before)) out.push_back(&e);
  return out;
}

const std::vector<Chain> &chains() {
  auto block = [](BlockType b) { return itemForBlock(b); };
  std::vector<ItemId> beds;
  for (int c = 0; c < 8; ++c) beds.push_back(bedItem(static_cast<BedColour>(c)));
  static const std::vector<Chain> list = {
      {"Settling in",
       {{{block(BlockType::CRAFTING_TABLE)}, {},
         "I can't make much with just my hands. Four planks make a Crafting Table.",
         "I made a Crafting Table!"},
        {beds, {},
         "It gets dark fast. A bed needs wool and planks. Woollies give wool for leaves...",
         "I made a bed!"},
        {{}, Deed::Slept,
         "Next time it gets dark, I'll sleep in my bed till morning.",
         "Slept like a log!"}},
       {block(BlockType::TORCH), 8}, "8 Torches", true},
      {"Hot stuff",
       {{{ItemId::RAW_IRON}, {},
         "There's iron in the stone somewhere. I'll need a copper pick to dig it.",
         "I found iron!"},
        {{ItemId::IRON_INGOT}, {},
         "I bet a furnace could melt this iron down. Eight cobblestone make one.",
         "Melted it into an ingot!"},
        {{ItemId::IRON_PICKAXE}, {},
         "Iron is tough. An iron pickaxe could dig the deepest gems.",
         "I made an iron pickaxe!"}},
       {ItemId::IRON_INGOT, 3}, "3 Iron Ingots"},
      {"Making friends",
       {{{}, Deed::FedAnimal,
         "The animals look hungry. Woollies love leaves.",
         "I fed an animal. It looked so happy!"},
        {{}, Deed::TamedPet,
         "I saw a stray Puppy. Three treats and it might stay with me!",
         "I have a pet!"},
        {{}, Deed::Sailed,
         "Five planks make a boat. I want to go sailing!",
         "I went for a boat ride!"}},
       {ItemId::BERRY_COOKIE, 3}, "3 Berry Cookies"},
      {"Green thumb",
       {{{}, Deed::Tilled,
         "A hoe turns grass into soil for planting.",
         "I tilled some soil!"},
        {{ItemId::WHEAT}, {},
         "Tall grass sometimes has seeds. Plant them and keep them watered.",
         "I grew wheat!"},
        {{ItemId::BREAD}, {},
         "Three wheat make a loaf of bread.",
         "I baked bread!"},
        {{block(BlockType::CAKE_4)}, {},
         "A cake needs wheat, strawberries and sweet petals from a Blossom Deer.",
         "I made a whole cake!"}},
       {ItemId::COMPOST, 4}, "4 Compost"},
      {"Gone fishing",
       {{{ItemId::FISHING_ROD}, {},
         "I could make a fishing rod and try my luck at the water.",
         "I made a fishing rod!"},
        {{ItemId::RAW_PEBBLEFISH, ItemId::RAW_SUNFIN, ItemId::RAW_GLOWFIN}, {},
         "Cast, wait for the bobber to dip, then reel in!",
         "I caught a fish!"},
        {{ItemId::COOKED_PEBBLEFISH, ItemId::COOKED_SUNFIN, ItemId::COOKED_GLOWFIN}, {},
         "Raw fish? A furnace can cook it.",
         "Cooked it. Yum!"},
        {{ItemId::RAW_GLOWFIN}, {},
         "They say a fish glows in the pools deep in caves...",
         "I caught a Glowfin! It glows!"}},
       {ItemId::TREASURE_KEY, 1}, "a Treasure Key"},
      {"Deep down",
       {{{block(BlockType::GLOW_CRYSTAL)}, {},
         "Deep caves shine with glow crystals. I want one!",
         "I found a glow crystal!"},
        {{}, Deed::OpenedTreasure,
         "There are locked treasure chests deep underground. A Treasure Key opens one.",
         "I opened a treasure chest!"},
        {{ItemId::DIAMOND}, {},
         "The deepest gem of all is a diamond. I'll need an iron pick.",
         "I found a diamond!"}},
       {ItemId::DIAMOND, 1}, "a Diamond"},
  };
  return list;
}

bool stepDone(const Step &step, const SeenItems &seen, const JournalProgress &j) {
  if (step.deed && j.did(*step.deed)) return true;
  for (ItemId id : step.any_of) {
    const auto v = static_cast<size_t>(id);
    if (v < seen.size() && seen.test(v)) return true;
  }
  return false;
}

int stepsDone(const Chain &chain, const SeenItems &seen, const JournalProgress &j) {
  int n = 0;
  while (n < static_cast<int>(chain.steps.size()) && stepDone(chain.steps[n], seen, j)) ++n;
  return n;
}

bool chainOpen(int index, const SeenItems &seen, const JournalProgress &j) {
  const auto &all = chains();
  if (index < 0 || index >= static_cast<int>(all.size())) return false;
  if (all[index].starts_open || j.stamps.test(static_cast<size_t>(index))) return true;
  return stepDone(all.front().steps.front(), seen, j);
}

bool chainComplete(const Chain &chain, const SeenItems &seen, const JournalProgress &j) {
  return stepsDone(chain, seen, j) == static_cast<int>(chain.steps.size());
}

std::vector<int> chainsToStamp(const SeenItems &seen, const JournalProgress &j) {
  std::vector<int> out;
  const auto &all = chains();
  for (int i = 0; i < static_cast<int>(all.size()); ++i)
    if (!j.stamps.test(static_cast<size_t>(i)) && chainOpen(i, seen, j) &&
        chainComplete(all[i], seen, j))
      out.push_back(i);
  return out;
}
