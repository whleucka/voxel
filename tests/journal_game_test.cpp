// The journal in the Game: deeds, creatures met, new finds, saving.
#include "doctest/doctest.h"
#include "block/block_data.hpp"
#include "entity/entity_store.hpp"
#include "sim/game.hpp"
#include "sim/local_connection.hpp"
#include "test_util.hpp"
#include "world/world_save.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>

namespace {

constexpr int kBase = 200;

std::vector<Event> pollAll(LocalConnection &conn) {
  std::vector<Event> out;
  conn.poll(out);
  return out;
}

template <class T> std::vector<T> all(const std::vector<Event> &evs) {
  std::vector<T> out;
  for (const Event &e : evs)
    if (auto *t = std::get_if<T>(&e)) out.push_back(*t);
  return out;
}

bool rejected(const std::vector<Event> &evs) { return !all<CommandRejected>(evs).empty(); }

bool noticed(const std::vector<Event> &evs, const std::string &text) {
  for (const Notice &n : all<Notice>(evs))
    if (n.text == text) return true;
  return false;
}

bool chatted(const std::vector<Event> &evs, const std::string &text) {
  for (const ChatMessage &m : all<ChatMessage>(evs))
    if (m.text == text) return true;
  return false;
}

// A stone field in the sky over chunks x 0..2, z 0; the host stands on it.
struct JournalGame {
  ScopedRenderDistance rd{1};
  Game game;
  LocalConnection host, guest;
  PlayerId hid = 0, gid = 0;
  static inline const glm::vec3 kStand{4.5f, kBase + 1.0f, 8.5f};

  JournalGame() {
    game.spawner() = Spawner{};
    hid = join(host, "", Game::Role::Host);
    for (int cx = 0; cx < 3; ++cx) game.world().loadChunkSync(cx, 0);
    fill(game.world(), {0, kBase - 2, 0}, {47, kBase, 15}, BlockType::STONE);
    fill(game.world(), {0, kBase + 1, 0}, {47, kBase + 12, 15}, BlockType::AIR);
    stand(host, hid, kStand);
  }

  PlayerId join(LocalConnection &conn, std::string name, Game::Role role) {
    game.addClient(conn, role);
    conn.send(Join{std::move(name)});
    game.pump();
    auto joined = all<Joined>(pollAll(conn));
    REQUIRE(joined.size() == 1);
    return joined[0].id;
  }

  PlayerState &ps(PlayerId id) { return *game.findPlayer(id); }
  std::vector<Event> run(LocalConnection &conn, Command cmd) {
    conn.send(std::move(cmd));
    game.pump();
    return pollAll(conn);
  }
  void stand(LocalConnection &conn, PlayerId id, glm::vec3 at) {
    MovePlayer m;
    m.position = at;
    m.respawn = ps(id).respawn;
    run(conn, m);
  }
  void hold(PlayerId id, ItemId item, uint16_t n = 1) {
    ps(id).inventory.set(ps(id).selected_slot, n ? ItemStack{item, n} : ItemStack{});
  }
  // A second of game time: the journal checks once a second.
  std::vector<Event> second(LocalConnection &conn) {
    game.advance(1.0f);
    return pollAll(conn);
  }
  EntityId spawn(EntityKind kind, glm::vec3 at) {
    auto id = game.entities().spawn(kind, at);
    REQUIRE(id);
    return *id;
  }
};

} // namespace

TEST_CASE_FIXTURE(JournalGame, "feeding an animal and tilling soil are deeds") {
  EntityId woolly = spawn(EntityKind::Woolly, kStand + glm::vec3(0, 0, -2));
  hold(hid, itemForBlock(BlockType::OAK_LEAF));
  REQUIRE_FALSE(rejected(run(host, InteractEntity{woolly})));
  CHECK(ps(hid).journal.did(Deed::FedAnimal));

  const glm::ivec3 soil{6, kBase, 8};
  game.world().setBlockAt(soil, BlockType::GRASS);
  hold(hid, ItemId::WOOD_HOE);
  REQUIRE_FALSE(rejected(run(host, UseItemOn{soil})));
  CHECK(ps(hid).journal.did(Deed::Tilled));
  CHECK_FALSE(ps(hid).journal.did(Deed::Slept));
}

TEST_CASE_FIXTURE(JournalGame, "taming a pet is a deed, and the owner is sent their journal") {
  EntityId pup = spawn(EntityKind::Puppy, kStand + glm::vec3(0, 0, -2));
  hold(hid, ItemId::BERRIES, 3);
  for (int i = 0; i < 3; ++i) {
    if (i) {
      Entity *e = game.entities().find(pup);
      const glm::vec3 at = e->body.position;
      game.advance(1.05f);
      e->body.position = at;
    }
    REQUIRE_FALSE(rejected(run(host, InteractEntity{pup})));
  }
  CHECK(ps(hid).journal.did(Deed::TamedPet));
  game.advance(0.05f);
  auto states = all<JournalState>(pollAll(host));
  REQUIRE_FALSE(states.empty());
  CHECK(fromWire(states.back()).did(Deed::TamedPet));
}

TEST_CASE_FIXTURE(JournalGame, "opening a treasure chest with a key is a deed") {
  const glm::ivec3 chest{6, kBase + 1, 8};
  game.world().setBlockAt(chest, BlockType::TREASURE_CHEST);
  hold(hid, ItemId::TREASURE_KEY);
  REQUIRE_FALSE(rejected(run(host, OpenContainer{chest})));
  CHECK(ps(hid).journal.did(Deed::OpenedTreasure));
}

TEST_CASE_FIXTURE(JournalGame, "creatures within 16 blocks are met, once a second") {
  spawn(EntityKind::Bunny, kStand + glm::vec3(10, 0, 0));
  spawn(EntityKind::Woolly, kStand + glm::vec3(30, 0, 0));
  spawn(EntityKind::Pebblefish, kStand + glm::vec3(2, 0, 0));
  second(host); // the first check only takes note
  auto evs = second(host);
  CHECK(ps(hid).journal.hasMet(EntityKind::Bunny));
  CHECK_FALSE(ps(hid).journal.hasMet(EntityKind::Woolly));
  CHECK_FALSE(ps(hid).journal.hasMet(EntityKind::Pebblefish));
}

TEST_CASE_FIXTURE(JournalGame, "a new find is noticed; what you had on joining isn't") {
  // The starter kit's glowstone isn't a journal entry, but coal is.
  run(host, GiveItem{ItemId::COAL, 1});
  CHECK_FALSE(noticed(second(host), "New in your journal: Coal"));
  run(host, GiveItem{ItemId::RAW_GLOWFIN, 1});
  auto evs = second(host);
  CHECK(noticed(evs, "New in your journal: Glowfin"));
  run(host, GiveItem{ItemId::RAW_GLOWFIN, 1});
  CHECK(all<Notice>(second(host)).empty());
}

TEST_CASE_FIXTURE(JournalGame, "filling a whole page tells everyone") {
  second(host);
  for (ItemId id : {ItemId::RAW_PEBBLEFISH, ItemId::RAW_SUNFIN, ItemId::RAW_GLOWFIN})
    run(host, GiveItem{id, 1});
  CHECK_FALSE(chatted(second(host), "Host found every fish!"));
  run(host, GiveItem{ItemId::SOGGY_BOOT, 1});
  CHECK(chatted(second(host), ps(hid).name + " found every fish!"));
}

TEST_CASE_FIXTURE(JournalGame, "creative worlds record nothing in the journal") {
  game.setMode(GameMode::Creative);
  spawn(EntityKind::Bunny, kStand + glm::vec3(3, 0, 0));
  second(host);
  second(host);
  run(host, GiveItem{ItemId::RAW_GLOWFIN, 1});
  auto evs = second(host);
  CHECK_FALSE(ps(hid).journal.hasMet(EntityKind::Bunny));
  CHECK(all<Notice>(evs).empty());
  EntityId woolly = spawn(EntityKind::Woolly, kStand + glm::vec3(0, 0, -2));
  hold(hid, itemForBlock(BlockType::OAK_LEAF));
  run(host, InteractEntity{woolly});
  CHECK(ps(hid).journal.deeds.none());
}

TEST_CASE_FIXTURE(JournalGame, "host and guest journals are saved and loaded") {
  ScopedSeed seed;
  TempDir dir;
  game.load(dir.path.string());
  gid = join(guest, "Sam", Game::Role::Guest);
  ps(hid).journal.set(Deed::Sailed);
  ps(hid).journal.meet(EntityKind::Mummy);
  ps(gid).journal.set(Deed::Slept);
  ps(gid).journal.stamps.set(2);
  const JournalProgress host_j = ps(hid).journal, guest_j = ps(gid).journal;
  REQUIRE(game.save());

  ScopedRenderDistance rd0{0};
  Game again;
  again.spawner() = Spawner{};
  REQUIRE(again.load(dir.path.string()));
  LocalConnection h2, g2;
  again.addClient(h2, Game::Role::Host);
  h2.send(Join{""});
  again.addClient(g2, Game::Role::Guest);
  g2.send(Join{"Sam"});
  again.pump();
  auto hj = all<Joined>(pollAll(h2)), gj = all<Joined>(pollAll(g2));
  REQUIRE(hj.size() == 1);
  REQUIRE(gj.size() == 1);
  CHECK(again.findPlayer(hj[0].id)->journal == host_j);
  CHECK(again.findPlayer(gj[0].id)->journal == guest_j);
}

TEST_CASE("journal.dat round-trips; none is an empty journal") {
  TempDir dir;
  JournalProgress j, back;
  CHECK_FALSE(WorldSave::loadJournal(dir.path.string(), back));
  j.set(Deed::Tilled);
  j.meet(EntityKind::CaveGrumble);
  j.stamps.set(5);
  REQUIRE(WorldSave::saveJournal(dir.path.string(), j));
  REQUIRE(WorldSave::loadJournal(dir.path.string(), back));
  CHECK(back == j);
}

TEST_CASE("a guest record from before the journal loads with an empty one") {
  TempDir dir;
  WorldSave::GuestRecord rec;
  rec.discovered = {ItemId::COAL};
  rec.journal.set(Deed::Sailed);
  REQUIRE(WorldSave::saveGuest(dir.path.string(), "Sam", rec));
  // Cut off the trailing journal section: 2 + 4 + 2 bytes.
  const auto file = dir.path / "players" / "Sam.dat";
  std::filesystem::resize_file(file, std::filesystem::file_size(file) - 8);
  std::map<std::string, WorldSave::GuestRecord> out;
  REQUIRE(WorldSave::loadGuests(dir.path.string(), out));
  REQUIRE(out.count("Sam"));
  CHECK(out["Sam"].discovered == rec.discovered);
  CHECK(out["Sam"].journal == JournalProgress{});
}
