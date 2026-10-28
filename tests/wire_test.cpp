#include "doctest/doctest.h"
#include "net/wire.hpp"
#include "item/item.hpp"
#include <cmath>
#include <cstring>
#include <limits>

namespace {

template <class V> std::vector<uint8_t> enc(const V &v) { return wire::encode(v); }

// Encoding what decoding produced gives the same bytes: nothing was lost.
void roundTrip(const Command &c) {
  auto bytes = enc(c);
  auto back = wire::decodeCommand(bytes);
  REQUIRE(back);
  CHECK(back->index() == c.index());
  CHECK(enc(*back) == bytes);
}
void roundTrip(const Event &e) {
  auto bytes = enc(e);
  auto back = wire::decodeEvent(bytes);
  REQUIRE(back);
  CHECK(back->index() == e.index());
  CHECK(enc(*back) == bytes);
}

template <class V, size_t... I> void everyDefault(std::index_sequence<I...>) {
  (roundTrip(V{std::in_place_index<I>}), ...);
}

EntitySnapshot richSnapshot() {
  EntitySnapshot s;
  s.tick = 123456;
  s.entities = {{7, EntityKind::Gloomling, {-1.5f, 64.25f, 1e6f}, 270.0f, ItemId::NONE, true, false},
                {8, EntityKind::DroppedItem, {0, 0, 0}, 0.0f, ItemId::DIAMOND, false, true}};
  s.players = {{2, "Bea", {-100.5f, 70.0f, 3.0f}, -45.0f, 12.5f, true},
               {3, "kid_2-b", {0, 0, 0}, 0.0f, 0.0f, false, true}};
  return s;
}

} // namespace

TEST_CASE("every command and event round-trips with default values") {
  everyDefault<Command>(std::make_index_sequence<std::variant_size_v<Command>>{});
  everyDefault<Event>(std::make_index_sequence<std::variant_size_v<Event>>{});
}

TEST_CASE("commands round-trip with real values") {
  roundTrip(Join{"Bea"});
  MovePlayer m{{-12345.5f, 255.9f, 0.001f}, -179.0f, 89.0f, true, true, true, true, true, true, 42};
  roundTrip(m);
  auto back = std::get<MovePlayer>(*wire::decodeCommand(enc(Command{m})));
  CHECK(back.position == m.position);
  CHECK(back.yaw == m.yaw);
  CHECK(back.respawn == 42);
  CHECK(back.eyes_underwater);
  roundTrip(BreakBlock{{-30000, 0, 30000}});
  roundTrip(PlaceBlock{{1, 255, -1}});
  roundTrip(SelectSlot{9});
  roundTrip(MoveItems{39, 0, 64});
  roundTrip(GiveItem{ItemId::SWEET_PETALS, 999});
  roundTrip(SpawnDebugEntity{EntityKind::CaveGrumble});
  roundTrip(AttackEntity{0xFFFFFFFF});
  roundTrip(DropItem{true});
  CHECK(std::get<SelectSlot>(*wire::decodeCommand(enc(Command{SelectSlot{-3}}))).slot == -3);
}

TEST_CASE("events round-trip with real values") {
  roundTrip(Joined{5, {1, 2, 3}, 10.0f, -20.0f, 7, true});
  roundTrip(Joined{5, {1, 2, 3}, 10.0f, -20.0f, 7, false, GameMode::Creative});
  roundTrip(BlockChanged{{-1, 100, -1}, BlockType::GLOWSTONE});
  roundTrip(BlockChanged{{3, 64, -7}, kLastBlockType});
  roundTrip(PlayerJoined{4, "Cy"});
  roundTrip(CommandRejected{"give: host only"});
  roundTrip(JoinRefused{"Name already in use"});

  InventoryContents inv;
  inv.slots[0] = {ItemId::COAL, 64};
  inv.slots[kInventorySlots - 1] = {itemForBlock(BlockType::STONE), 1};
  roundTrip(inv);
  auto inv_back = std::get<InventoryContents>(*wire::decodeEvent(enc(Event{inv})));
  CHECK(inv_back.slots == inv.slots);

  roundTrip(VitalsChanged{0, 20, 7, false, true});
  roundTrip(Respawned{{8, 70, 8}, 3});
  roundTrip(KnockedBack{{-7, 5, 0.5f}});
  roundTrip(TimeSync{43200.0f, 35.0f});

  EntitySnapshot snap = richSnapshot();
  roundTrip(snap);
  auto snap_back = std::get<EntitySnapshot>(*wire::decodeEvent(enc(Event{snap})));
  REQUIRE(snap_back.players.size() == 2);
  CHECK(snap_back.players[0].name == "Bea");
  CHECK(snap_back.players[0].crouch);
  CHECK_FALSE(snap_back.players[0].sleeping);
  CHECK(snap_back.players[1].sleeping);
  CHECK(snap_back.entities[1].item == ItemId::DIAMOND);
  CHECK(snap_back.entities[0].kind == EntityKind::Gloomling);
}

TEST_CASE("a welcome with many edits round-trips") {
  Welcome w{0xDEADBEEF, {123.5f, -9.0f}, 3600.0f, 72.0f, {}, worldgen::kCaves};
  for (int i = 0; i < 50000; ++i)
    w.edits.push_back({{i - 25000, i % 256, -i}, static_cast<BlockType>(i % 4)});
  roundTrip(w);
  auto back = std::get<Welcome>(*wire::decodeEvent(enc(Event{w})));
  CHECK(back.edits == w.edits);
  CHECK(back.seed == w.seed);
  CHECK(back.worldgen == worldgen::kCaves);
  CHECK(enc(Event{w}).size() < wire::kMaxFrame);
}

TEST_CASE("truncated and padded payloads are rejected") {
  auto bytes = enc(Event{richSnapshot()});
  for (size_t n = 0; n < bytes.size(); ++n)
    CHECK_FALSE(wire::decodeEvent(std::span(bytes).first(n)));
  bytes.push_back(0);
  CHECK_FALSE(wire::decodeEvent(bytes));
}

TEST_CASE("garbage is rejected, not trusted") {
  CHECK_FALSE(wire::decodeCommand(std::vector<uint8_t>{}));
  CHECK_FALSE(wire::decodeCommand(std::vector<uint8_t>{200}));  // unknown index
  CHECK_FALSE(wire::decodeEvent(std::vector<uint8_t>{255, 1, 2}));

  // A non-finite float.
  auto move = enc(Command{MovePlayer{}});
  float nan = std::numeric_limits<float>::quiet_NaN();
  std::memcpy(move.data() + 1, &nan, 4);
  CHECK_FALSE(wire::decodeCommand(move));

  // An unknown block type.
  auto changed = enc(Event{BlockChanged{{0, 0, 0}, BlockType::STONE}});
  changed.back() = 250;
  CHECK_FALSE(wire::decodeEvent(changed));

  // An unknown item and an unknown entity kind.
  auto give = enc(Command{GiveItem{ItemId::COAL, 1}});
  give[1] = 0xFF, give[2] = 0xFE;
  CHECK_FALSE(wire::decodeCommand(give));
  auto spawn = enc(Command{SpawnDebugEntity{}});
  spawn[1] = 0xFF;
  CHECK_FALSE(wire::decodeCommand(spawn));

  // A bool that isn't 0 or 1.
  auto drop = enc(Command{DropItem{true}});
  drop[1] = 7;
  CHECK_FALSE(wire::decodeCommand(drop));

  // A list claiming far more elements than there are bytes.
  auto snap = enc(Event{EntitySnapshot{}});
  snap[5] = 0xFF, snap[6] = 0xFF, snap[7] = 0xFF, snap[8] = 0x7F;
  CHECK_FALSE(wire::decodeEvent(snap));
}

TEST_CASE("frames reassemble however the bytes arrive") {
  std::vector<std::vector<uint8_t>> payloads = {enc(Command{Join{"Bea"}}),
                                                enc(Command{MovePlayer{}}),
                                                {}, // an empty payload is still a frame
                                                enc(Command{EatItem{}})};
  std::vector<uint8_t> stream;
  for (const auto &p : payloads) {
    auto f = wire::frame(p);
    stream.insert(stream.end(), f.begin(), f.end());
  }

  SUBCASE("one byte at a time") {
    wire::FrameReader r;
    std::vector<std::vector<uint8_t>> got;
    for (uint8_t b : stream) {
      r.feed(std::span(&b, 1));
      while (auto p = r.next()) got.push_back(*p);
    }
    CHECK(got == payloads);
  }
  SUBCASE("all at once") {
    wire::FrameReader r;
    r.feed(stream);
    std::vector<std::vector<uint8_t>> got;
    while (auto p = r.next()) got.push_back(*p);
    CHECK(got == payloads);
    CHECK_FALSE(r.bad());
  }
}

TEST_CASE("an oversized frame marks the stream bad") {
  wire::FrameReader r;
  uint32_t huge = wire::kMaxFrame + 1;
  uint8_t len[4] = {uint8_t(huge), uint8_t(huge >> 8), uint8_t(huge >> 16), uint8_t(huge >> 24)};
  r.feed(len);
  CHECK_FALSE(r.next());
  CHECK(r.bad());
}

TEST_CASE("the hello frame carries the protocol version") {
  CHECK(wire::readHello(wire::hello()) == wire::kProtocolVersion);
  auto other = wire::hello();
  other[4] = 99;
  CHECK(wire::readHello(other) == 99);
  auto wrong = wire::hello();
  wrong[0] = 'X';
  CHECK_FALSE(wire::readHello(wrong));
  CHECK_FALSE(wire::readHello(enc(Command{Join{}})));
}

TEST_CASE("crafting messages round-trip") {
  roundTrip(Craft{15, true});
  CHECK(std::get<Craft>(*wire::decodeCommand(enc(Command{Craft{7, false}}))).recipe == 7);
  KnownItems k{{ItemId::STICKS, itemForBlock(BlockType::CRAFTING_TABLE), ItemId::RUBY}};
  auto back = std::get<KnownItems>(*wire::decodeEvent(enc(Event{k})));
  CHECK(back.ids == k.ids);
}

TEST_CASE("container messages round-trip") {
  roundTrip(OpenContainer{{-4, 70, 12}});
  roundTrip(CloseContainer{});
  roundTrip(UseDoor{{-4, 70, 12}});
  ContainerView v;
  v.pos = {1, 2, 3};
  v.open = true;
  v.input = {ItemId::RAW_IRON, 5};
  v.output = {ItemId::IRON_INGOT, 64};
  v.burn = 200;
  v.cook = 17;
  auto back = std::get<ContainerView>(*wire::decodeEvent(enc(Event{v})));
  CHECK(back.pos == v.pos);
  CHECK(back.input == v.input);
  CHECK(back.output == v.output);
  CHECK(back.burn == 200);
  CHECK(back.cook == 17);
}

TEST_CASE("wear travels with stacks") {
  Inventory inv;
  inv.set(4, {ItemId::DIAMOND, 1, 321});
  auto back = std::get<InventoryContents>(*wire::decodeEvent(enc(Event{InventoryContents{inv.slots()}})));
  CHECK(back.slots[4] == ItemStack{ItemId::DIAMOND, 1, 321});
}

TEST_CASE("mining messages round-trip") {
  roundTrip(StartBreak{{7, 64, -3}});
  CHECK(std::get<ToolBroke>(*wire::decodeEvent(enc(Event{ToolBroke{ItemId::IRON_AXE}}))).item ==
        ItemId::IRON_AXE);
}

TEST_CASE("burning travels in entity snapshots") {
  EntitySnapshot snap;
  EntityView v;
  v.id = 3;
  v.burning = true;
  snap.entities.push_back(v);
  auto back = std::get<EntitySnapshot>(*wire::decodeEvent(enc(Event{snap})));
  REQUIRE(back.entities.size() == 1);
  CHECK(back.entities[0].burning);
}

TEST_CASE("fishing messages round-trip") {
  auto cast = std::get<CastLine>(*wire::decodeCommand(enc(Command{CastLine{{0.5f, 0.25f, -1.0f}}})));
  CHECK(cast.dir == glm::vec3(0.5f, 0.25f, -1.0f));
  CHECK(std::holds_alternative<ReelIn>(*wire::decodeCommand(enc(Command{ReelIn{}}))));
  CHECK(std::get<Caught>(*wire::decodeEvent(enc(Event{Caught{ItemId::RAW_SUNFIN}}))).item ==
        ItemId::RAW_SUNFIN);

  EntitySnapshot snap;
  EntityView v;
  v.kind = EntityKind::Bobber;
  v.owner = 7;
  v.biting = true;
  snap.entities.push_back(v);
  auto back = std::get<EntitySnapshot>(*wire::decodeEvent(enc(Event{snap})));
  REQUIRE(back.entities.size() == 1);
  CHECK(back.entities[0].owner == 7);
  CHECK(back.entities[0].biting);
}

TEST_CASE("chest views round-trip with their kind and slots") {
  ContainerView v;
  v.pos = {3, 40, -9};
  v.kind = ContainerKind::Chest;
  v.slots.assign(kChestSlots, ItemStack{});
  v.slots[4] = {ItemId::TREASURE_KEY, 2};
  v.slots[19] = {ItemId::RUBY_SWORD, 1, 12};
  auto back = std::get<ContainerView>(*wire::decodeEvent(enc(Event{v})));
  CHECK(back.kind == ContainerKind::Chest);
  CHECK(back.slots == v.slots);
}

TEST_CASE("bed messages round-trip") {
  roundTrip(UseBed{{-4, 70, 12}});
  roundTrip(LeaveBed{});
  roundTrip(SleepState{true, {-4, 70, 12}, 2, 3});
  roundTrip(Notice{"You can't sleep, monsters are nearby!"});
  auto back = std::get<SleepState>(*wire::decodeEvent(enc(Event{SleepState{true, {-4, 70, 12}, 2, 3}})));
  CHECK(back == SleepState{true, {-4, 70, 12}, 2, 3});
}
