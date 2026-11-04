#include "doctest/doctest.h"
#include "block/block_type.hpp"
#include "item/item.hpp"
#include "journal/journal.hpp"
#include <set>

TEST_CASE("journal pages list the spec's entries") {
  SeenItems none;
  JournalProgress j;
  CHECK(pageCount(Page::Creatures, none, j).total == 11);
  CHECK(pageCount(Page::Fish, none, j).total == 4);
  CHECK(pageCount(Page::GemsOres, none, j).total == 7);
  CHECK(pageCount(Page::Plants, none, j).total == 17);
  for (int p = 0; p < kPageCount; ++p)
    CHECK(pageCount(static_cast<Page>(p), none, j).filled == 0);
}

TEST_CASE("every journal entry is real, named, hinted and listed once") {
  std::set<std::string_view> names;
  std::set<int> portraits;
  for (const Entry &e : entries()) {
    CHECK(!entryName(e).empty());
    CHECK(!e.hint.empty());
    CHECK(names.insert(entryName(e)).second);
    if (const auto *k = std::get_if<EntityKind>(&e.what)) {
      CHECK(e.page == Page::Creatures);
      CHECK(isJournalCreature(*k));
      CHECK(e.portrait >= 0);
      CHECK(portraits.insert(e.portrait).second);
    } else {
      ItemId id = std::get<ItemId>(e.what);
      CHECK(isValidItem(id));
      CHECK(static_cast<size_t>(id) < kItemIdSpace);
    }
  }
}

TEST_CASE("every journal item can be got in survival") {
  // Each item entry drops from a block, or comes from fishing or an animal.
  for (const Entry &e : entries()) {
    const auto *id = std::get_if<ItemId>(&e.what);
    if (!id) continue;
    CAPTURE(itemDef(*id).name);
    bool from_block = false;
    for (int b = 1; b < 256; ++b)
      if (dropFor(static_cast<BlockType>(b)) == *id) from_block = true;
    const bool other = *id == ItemId::RAW_PEBBLEFISH || *id == ItemId::RAW_SUNFIN ||
                       *id == ItemId::RAW_GLOWFIN || *id == ItemId::SOGGY_BOOT ||
                       *id == ItemId::SWEET_PETALS;
    CHECK((from_block || other));
  }
}

TEST_CASE("item entries fill from seen items, creatures from meeting") {
  SeenItems seen;
  JournalProgress j;
  see(seen, ItemId::RAW_GLOWFIN);
  see(seen, itemForBlock(BlockType::DAISY));
  j.meet(EntityKind::Zombie);
  CHECK(pageCount(Page::Fish, seen, j).filled == 1);
  CHECK(pageCount(Page::Plants, seen, j).filled == 1);
  CHECK(pageCount(Page::Creatures, seen, j).filled == 1);
  CHECK(pageCount(Page::GemsOres, seen, j).filled == 0);
}

TEST_CASE("newlyFilled lists only the new finds") {
  SeenItems before, now;
  JournalProgress jb, jn;
  see(before, ItemId::COAL);
  now = before;
  see(now, ItemId::DIAMOND);
  jn.meet(EntityKind::Bunny);
  auto fresh = newlyFilled(before, jb, now, jn);
  REQUIRE(fresh.size() == 2);
  CHECK(entryName(*fresh[0]) == "Bunny");
  CHECK(entryName(*fresh[1]) == "Diamond");
}

TEST_CASE("deeds and meetings set and test") {
  JournalProgress j;
  CHECK_FALSE(j.did(Deed::Sailed));
  j.set(Deed::Sailed);
  CHECK(j.did(Deed::Sailed));
  CHECK_FALSE(j.did(Deed::Slept));
  CHECK_FALSE(isJournalCreature(EntityKind::Boat));
  CHECK_FALSE(isJournalCreature(EntityKind::Pebblefish));
  j.meet(EntityKind::Kitten);
  CHECK(j.hasMet(EntityKind::Kitten));
  CHECK_FALSE(j.hasMet(EntityKind::Puppy));
}
