#include "doctest/doctest.h"
#include "block/block_type.hpp"
#include "item/item.hpp"
#include "journal/journal.hpp"
#include "item/recipes.hpp"
#include <set>

TEST_CASE("journal pages list the spec's entries") {
  SeenItems none;
  JournalProgress j;
  CHECK(pageCount(JournalPage::Creatures, none, j).total == 11);
  CHECK(pageCount(JournalPage::Fish, none, j).total == 4);
  CHECK(pageCount(JournalPage::GemsOres, none, j).total == 7);
  CHECK(pageCount(JournalPage::Plants, none, j).total == 17);
  for (int p = 0; p < kPageCount; ++p)
    CHECK(pageCount(static_cast<JournalPage>(p), none, j).filled == 0);
}

TEST_CASE("every journal entry is real, named, hinted and listed once") {
  std::set<std::string_view> names;
  std::set<int> portraits;
  for (const Entry &e : entries()) {
    CHECK(!entryName(e).empty());
    CHECK(!e.hint.empty());
    CHECK(names.insert(entryName(e)).second);
    if (const auto *k = std::get_if<EntityKind>(&e.what)) {
      CHECK(e.page == JournalPage::Creatures);
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
  CHECK(pageCount(JournalPage::Fish, seen, j).filled == 1);
  CHECK(pageCount(JournalPage::Plants, seen, j).filled == 1);
  CHECK(pageCount(JournalPage::Creatures, seen, j).filled == 1);
  CHECK(pageCount(JournalPage::GemsOres, seen, j).filled == 0);
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

TEST_CASE("six chains, each with steps, notes and a gift") {
  REQUIRE(chains().size() == 6);
  CHECK(chains()[0].starts_open);
  for (size_t i = 1; i < chains().size(); ++i) CHECK_FALSE(chains()[i].starts_open);
  for (const Chain &c : chains()) {
    CAPTURE(c.title);
    CHECK(c.steps.size() >= 3);
    CHECK_FALSE(c.gift.empty());
    CHECK(isValidItem(c.gift.id));
    CHECK_FALSE(c.gift_text.empty());
    for (const Step &s : c.steps) {
      CHECK((s.deed || !s.any_of.empty()));
      CHECK_FALSE(s.hint.empty());
      CHECK_FALSE(s.done.empty());
      for (ItemId id : s.any_of) CHECK(isValidItem(id));
    }
  }
}

TEST_CASE("steps tick in order; one already done ticks when its turn comes") {
  const Chain &hot = chains()[1];
  SeenItems seen;
  JournalProgress j;
  CHECK(stepsDone(hot, seen, j) == 0);
  see(seen, ItemId::IRON_PICKAXE); // the last step first
  CHECK(stepsDone(hot, seen, j) == 0);
  see(seen, ItemId::RAW_IRON);
  CHECK(stepsDone(hot, seen, j) == 1);
  see(seen, ItemId::IRON_INGOT);
  CHECK(stepsDone(hot, seen, j) == 3);
  CHECK(chainComplete(hot, seen, j));
}

TEST_CASE("any of a step's items, or its deed, does it") {
  const Chain &fishing = chains()[4];
  SeenItems seen;
  JournalProgress j;
  see(seen, ItemId::FISHING_ROD);
  see(seen, ItemId::RAW_SUNFIN);
  CHECK(stepsDone(fishing, seen, j) == 2);
  const Chain &friends = chains()[2];
  j.set(Deed::FedAnimal);
  CHECK(stepsDone(friends, seen, j) == 1);
}

TEST_CASE("chains open after the first step of Settling in, and stamp once") {
  SeenItems seen;
  JournalProgress j;
  CHECK(chainOpen(0, seen, j));
  CHECK_FALSE(chainOpen(1, seen, j));
  // Hot stuff done before anything else: not open yet, so nothing to stamp.
  for (ItemId id : {ItemId::RAW_IRON, ItemId::IRON_INGOT, ItemId::IRON_PICKAXE}) see(seen, id);
  CHECK(chainsToStamp(seen, j).empty());
  see(seen, itemForBlock(BlockType::CRAFTING_TABLE));
  CHECK(chainOpen(1, seen, j));
  CHECK(chainsToStamp(seen, j) == std::vector<int>{1});
  j.stamps.set(1);
  CHECK(chainsToStamp(seen, j).empty());
}

TEST_CASE("every chain gift and step item can be got in survival") {
  // Each is made by a recipe, smelted, dropped by a block, or caught.
  auto made = [](ItemId id) {
    for (const Recipe &r : recipes())
      if (r.makes.id == id) return true;
    return false;
  };
  auto dropped = [](ItemId id) {
    for (int b = 1; b < 256; ++b)
      if (dropFor(static_cast<BlockType>(b)) == id) return true;
    return false;
  };
  auto ok = [&](ItemId id) {
    return made(id) || dropped(id) || id == ItemId::IRON_INGOT || id == ItemId::WHEAT ||
           id == ItemId::RAW_PEBBLEFISH || id == ItemId::RAW_SUNFIN ||
           id == ItemId::RAW_GLOWFIN || id == ItemId::COOKED_PEBBLEFISH ||
           id == ItemId::COOKED_SUNFIN || id == ItemId::COOKED_GLOWFIN;
  };
  for (const Chain &c : chains()) {
    CAPTURE(c.title);
    CHECK(ok(c.gift.id));
    for (const Step &s : c.steps)
      for (ItemId id : s.any_of) {
        CAPTURE(itemDef(id).name);
        CHECK(ok(id));
      }
  }
}
