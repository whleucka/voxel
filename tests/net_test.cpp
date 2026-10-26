#include "doctest/doctest.h"
#include "net/net_connection.hpp"
#include "net/net_server.hpp"
#include "sim/game.hpp"
#include "sim/local_connection.hpp"
#include "test_util.hpp"
#include <chrono>
#include <functional>
#include <thread>

namespace {

// A host Game on 127.0.0.1 with its own in-process player, plus helpers to
// run everything until something arrives.
struct LoopbackHost {
  ScopedRenderDistance rd{0};
  Game game;
  // Declared before the server: the server's goodbye on destruction still
  // reaches the Game's other links.
  LocalConnection host;
  std::vector<Event> host_events;
  NetServer server{game};
  int ground = 0;

  LoopbackHost() {
    game.spawner() = Spawner{};
    std::string error;
    REQUIRE(server.listen(0, error));
    game.world().loadChunkSync(0, 0);
    for (int y = kChunkHeight - 1; y >= 0; --y)
      if (game.world().isSolidBlock(5, y, 5)) {
        ground = y;
        break;
      }
    game.addClient(host, Game::Role::Host);
    host.send(Join{"Host"});
    game.pump();
    host.poll(host_events);
  }

  // One host frame: network in, game, network out.
  void step() {
    server.poll();
    game.pump();
    game.advance(Game::kTickSeconds);
    server.flush();
    host.poll(host_events);
  }

  // Steps until `done` holds for the guest's received events (2 s at most).
  bool until(NetConnection &guest, std::vector<Event> &got,
             const std::function<bool(const std::vector<Event> &)> &done) {
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (std::chrono::steady_clock::now() < deadline) {
      step();
      guest.poll(got);
      if (done(got)) return true;
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return false;
  }

  template <class T> static const T *find(const std::vector<Event> &evs) {
    for (const Event &e : evs)
      if (auto *t = std::get_if<T>(&e)) return t;
    return nullptr;
  }
  template <class T> static bool has(const std::vector<Event> &evs) { return find<T>(evs); }

  // Connects and joins a guest; returns its Joined.
  Joined join(NetConnection &guest, std::vector<Event> &got, const std::string &name) {
    std::string error;
    REQUIRE(guest.connect("127.0.0.1", server.port(), error));
    guest.send(Join{name});
    REQUIRE(until(guest, got, has<Joined>));
    return *find<Joined>(got);
  }
};

} // namespace

TEST_CASE_FIXTURE(LoopbackHost, "a guest joins over TCP: welcome, then joined") {
  NetConnection guest;
  std::vector<Event> got;
  Joined j = join(guest, got, "Bea");
  CHECK(game.findPlayer(j.id)->name == "Bea");
  REQUIRE(std::holds_alternative<Welcome>(got[0]));
  CHECK(std::get<Welcome>(got[0]).seed == g_settings.world_seed);
  CHECK(server.guests() == 1);
  CHECK(has<PlayerJoined>(host_events));
}

TEST_CASE_FIXTURE(LoopbackHost, "a guest's edit reaches the host's world and everyone hears") {
  NetConnection guest;
  std::vector<Event> got;
  join(guest, got, "Bea");
  guest.send(MovePlayer{{5.5f, ground + 1.0f, 8.5f}});
  guest.send(BreakBlock{{5, ground, 5}});
  got.clear();
  host_events.clear();
  REQUIRE(until(guest, got, has<BlockChanged>));
  CHECK(find<BlockChanged>(got)->pos == glm::ivec3(5, ground, 5));
  CHECK(game.world().getBlockAt({5.5f, ground + 0.5f, 5.5f}) == BlockType::AIR);
  CHECK(has<BlockChanged>(host_events));
  CHECK(has<InventoryContents>(got)); // the drop went into Bea's inventory
}

TEST_CASE_FIXTURE(LoopbackHost, "guests see the host in snapshots") {
  NetConnection guest;
  std::vector<Event> got;
  join(guest, got, "Bea");
  got.clear();
  REQUIRE(until(guest, got, [](const std::vector<Event> &evs) {
    for (const Event &e : evs)
      if (auto *s = std::get_if<EntitySnapshot>(&e); s && !s->players.empty()) return true;
    return false;
  }));
}

TEST_CASE_FIXTURE(LoopbackHost, "a guest who disconnects leaves the game and is remembered") {
  PlayerId id;
  {
    NetConnection guest;
    std::vector<Event> got;
    id = join(guest, got, "Bea").id;
  } // the socket closes here
  host_events.clear();
  for (int i = 0; i < 200 && game.findPlayer(id); ++i) {
    step();
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  CHECK_FALSE(game.findPlayer(id));
  CHECK(server.guests() == 0);
  CHECK(has<PlayerLeft>(host_events));
  CHECK(game.guestRecord("Bea"));
}

TEST_CASE_FIXTURE(LoopbackHost, "a taken name is refused and the link closes") {
  NetConnection guest;
  std::vector<Event> got;
  std::string error;
  REQUIRE(guest.connect("127.0.0.1", server.port(), error));
  guest.send(Join{"Host"});
  REQUIRE(until(guest, got, has<JoinRefused>));
  CHECK(find<JoinRefused>(got)->reason == "Name already in use");
  for (int i = 0; i < 50; ++i) step();
  CHECK(server.guests() == 0);
  CHECK(game.playerCount() == 1);
}

TEST_CASE_FIXTURE(LoopbackHost, "when the host quits, guests are told why") {
  NetConnection guest;
  std::vector<Event> got;
  join(guest, got, "Bea");
  server.shutdown("Host left the game");
  got.clear();
  for (int i = 0; i < 200 && !has<Disconnected>(got); ++i) {
    guest.poll(got);
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  REQUIRE(has<Disconnected>(got));
  CHECK(find<Disconnected>(got)->reason == "Host left the game");
  CHECK_FALSE(guest.open());
  CHECK(game.playerCount() == 1);
}

TEST_CASE("a lost host shows up as one Disconnected") {
  std::string error;
  net::Socket listener = net::listenTcp(0, error);
  REQUIRE(listener.valid());
  NetConnection guest;
  REQUIRE(guest.connect("127.0.0.1", net::localPort(listener), error));
  net::Socket accepted;
  for (int i = 0; i < 1000 && !accepted.valid(); ++i) accepted = net::acceptTcp(listener);
  REQUIRE(accepted.valid());
  {
    net::Stream host(std::move(accepted));
    host.send(wire::hello());
    host.flush();
  } // the host vanishes without a goodbye

  std::vector<Event> got;
  for (int i = 0; i < 200; ++i) {
    guest.poll(got);
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  int disconnects = 0;
  for (const Event &e : got) disconnects += std::holds_alternative<Disconnected>(e);
  CHECK(disconnects == 1);
}

TEST_CASE_FIXTURE(LoopbackHost, "a guest sending garbage is dropped; the host carries on") {
  std::string error;
  net::Stream raw(net::connectTcp("127.0.0.1", server.port(), 1000, error));
  REQUIRE(raw.open());
  raw.send(wire::hello());
  std::vector<uint8_t> junk = {250, 1, 2, 3};
  raw.send(junk);
  raw.flush();
  for (int i = 0; i < 100 && server.guests() < 1; ++i) step();
  for (int i = 0; i < 100 && server.guests() > 0; ++i) {
    step();
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  CHECK(server.guests() == 0);
  CHECK(game.playerCount() == 1);

  NetConnection guest; // and others can still join
  std::vector<Event> got;
  join(guest, got, "Bea");
}

TEST_CASE_FIXTURE(LoopbackHost, "a guest from a different version is turned away") {
  std::string error;
  net::Stream raw(net::connectTcp("127.0.0.1", server.port(), 1000, error));
  REQUIRE(raw.open());
  auto old = wire::hello();
  old[4] = 99;
  raw.send(old);
  raw.flush();
  for (int i = 0; i < 100 && server.guests() < 1; ++i) step();
  for (int i = 0; i < 100 && server.guests() > 0; ++i) {
    step();
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  CHECK(server.guests() == 0);
}

TEST_CASE("a guest reaching a host of a different version says so") {
  std::string error;
  net::Socket listener = net::listenTcp(0, error);
  REQUIRE(listener.valid());
  NetConnection guest;
  REQUIRE(guest.connect("127.0.0.1", net::localPort(listener), error));
  net::Socket accepted;
  for (int i = 0; i < 1000 && !accepted.valid(); ++i) accepted = net::acceptTcp(listener);
  REQUIRE(accepted.valid());
  net::Stream fake_host(std::move(accepted));
  auto other = wire::hello();
  other[4] = 99;
  fake_host.send(other);
  fake_host.flush();

  std::vector<Event> got;
  for (int i = 0; i < 200 && got.empty(); ++i) {
    guest.poll(got);
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  REQUIRE(got.size() == 1);
  REQUIRE(std::holds_alternative<Disconnected>(got[0]));
  CHECK(std::get<Disconnected>(got[0]).reason == "Different game version");
}

TEST_CASE("connecting to nobody fails with a reason") {
  std::string error;
  net::Socket listener = net::listenTcp(0, error);
  uint16_t port = net::localPort(listener);
  listener.close(); // nothing is listening there now
  NetConnection guest;
  CHECK_FALSE(guest.connect("127.0.0.1", port, error, 500));
  CHECK_FALSE(error.empty());
}
