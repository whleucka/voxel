#include "journal/journal.hpp"
#include "block/block_type.hpp"

bool JournalProgress::hasMet(EntityKind k) const {
  const auto v = static_cast<size_t>(k);
  return v < met.size() && met.test(v);
}

void JournalProgress::meet(EntityKind k) {
  const auto v = static_cast<size_t>(k);
  if (v < met.size()) met.set(v);
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

std::string_view pageTitle(Page p) {
  switch (p) {
    case Page::Creatures: return "Creatures";
    case Page::Fish:      return "Fish";
    case Page::GemsOres:  return "Gems & Ores";
    case Page::Plants:    return "Plants";
  }
  return "";
}

const std::vector<Entry> &entries() {
  using enum Page;
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

      {Fish, ItemId::RAW_PEBBLEFISH, "Lakes and rivers. Catch one with a rod.", -1},
      {Fish, ItemId::RAW_SUNFIN, "Out in the sea.", -1},
      {Fish, ItemId::RAW_GLOWFIN, "Cave pools. It glows!", -1},
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
  if (const auto *k = std::get_if<EntityKind>(&e.what)) return entityType(*k).name;
  return itemDef(std::get<ItemId>(e.what)).name;
}

bool entryFilled(const Entry &e, const SeenItems &seen, const JournalProgress &j) {
  if (const auto *k = std::get_if<EntityKind>(&e.what)) return j.hasMet(*k);
  const auto v = static_cast<size_t>(std::get<ItemId>(e.what));
  return v < seen.size() && seen.test(v);
}

PageCount pageCount(Page p, const SeenItems &seen, const JournalProgress &j) {
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
