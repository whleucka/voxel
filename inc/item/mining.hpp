#pragma once

#include "block/block_type.hpp"
#include "item/tools.hpp"

// How long a block takes to break and whether it drops anything, given
// what's in hand. Survival only: creative breaks anything in one hit. Pure,
// so the client's hold-to-mine timer and the Game's check always agree.

struct Toughness {
  bool breakable = true;
  int hand_ms = 0;                  // with bare hands (or the wrong tool)
  ToolKind best = ToolKind::None;   // the tool that speeds it up
  ToolKind also = ToolKind::None;   // a second one that does (swords on leaves)
  Tier needs = Tier::Hand;          // to drop anything, a `best` tool of at least this
};

Toughness toughness(BlockType block);

// Milliseconds to break `block` holding `held` (NONE: bare hands); -1 if it
// can't be broken at all.
int breakMs(BlockType block, ItemId held);

// Whether breaking `block` with `held` gives its drop.
bool dropsWith(BlockType block, ItemId held);

// Coal, copper, iron, gold, ruby, emerald and diamond ore.
bool isOre(BlockType block);

// Ore won't break for a pickaxe too weak to get its drop, so nothing is
// wasted: the weakest pickaxe that will, when `held` isn't good enough;
// NONE when it is, or `block` isn't ore.
ItemId pickaxeNeeded(BlockType block, ItemId held);
