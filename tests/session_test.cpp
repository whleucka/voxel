#include "doctest/doctest.h"
#include "core/session.hpp"
#include "core/settings.hpp"
#include "test_util.hpp"
#include <chrono>
#include <functional>
#include <thread>

namespace {

struct Sessions {
  ScopedRenderDistance rd{1};
  ScopedSeed seed;
  TempDir dir;
  std::unique_ptr<Session> host;
  std::vector<Event> host_events;

  Sessions() {
    g_settings.world_seed = 1234;
    std::string error;
    host = Session::local(Session::Mode::Host, options("Host", 0), error);
    REQUIRE(host);
    host->game()->spawner() = Spawner{};
  }

  Session::Options options(std::string name, uint16_t port) {
    Session::Options o;
    o.name = std::move(name);
    o.save_dir = dir.path.string();
    o.world_name = "test";
    o.port = port;
    o.beacon = false;
    return o;
  }

  std::unique_ptr<Session> join(const std::string &name) {
    std::string error;
    auto s = Session::join("127.0.0.1", options(name, host->server()->port()), error);
    REQUIRE(s);
    return s;
  }

  // Runs both sides until `done` holds for the guest's events (3 s at most).
  bool until(Session &guest, std::vector<Event> &got,
             const std::function<bool(const std::vector<Event> &)> &done) {
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (std::chrono::steady_clock::now() < deadline) {
      host->frame(Game::kTickSeconds, 0.0);
      host->poll(host_events);
      guest.frame(Game::kTickSeconds, 0.0);
      guest.poll(got);
      if (done(got)) return true;
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return false;
  }

  template <class T> static bool has(const std::vector<Event> &evs) {
    for (const Event &e : evs)
      if (std::holds_alternative<T>(e)) return true;
    return false;
  }
};

} // namespace

TEST_CASE_FIXTURE(Sessions, "single player has no server and its own Game") {
  std::string error;
  TempDir other;
  Session::Options o = options("Solo", 0);
  o.save_dir = other.path.string();
  auto solo = Session::local(Session::Mode::Single, o, error);
  REQUIRE(solo);
  CHECK_FALSE(solo->server());
  REQUIRE(solo->game());
  std::vector<Event> evs;
  solo->poll(evs);
  CHECK(has<Joined>(evs));
}

TEST_CASE_FIXTURE(Sessions, "a guest session builds the host's world and clock") {
  host->game()->world().loadChunkSync(0, 0);
  host->game()->world().setBlockAt({3, 200, 3}, BlockType::GLOWSTONE);
  g_settings.world_seed = 999; // the guest's own seed, before joining

  auto guest = join("Bea");
  std::vector<Event> got;
  REQUIRE(until(*guest, got, has<Joined>));
  CHECK(g_settings.world_seed == 1234); // the host's, while playing there
  CHECK(guest->clock().time_of_day == doctest::Approx(host->clock().time_of_day).epsilon(0.01));
  auto edits = guest->world().editList();
  CHECK(std::find(edits.begin(), edits.end(), BlockEdit{{3, 200, 3}, BlockType::GLOWSTONE}) !=
        edits.end());

  guest.reset();
  CHECK(g_settings.world_seed == 999); // put back
}

TEST_CASE_FIXTURE(Sessions, "edits made after joining reach the guest's world") {
  auto guest = join("Bea");
  std::vector<Event> got;
  REQUIRE(until(*guest, got, has<Joined>));
  host->game()->world().loadChunkSync(0, 0);
  int ground = 0;
  for (int y = kChunkHeight - 1; y >= 0 && !ground; --y)
    if (host->game()->world().isSolidBlock(5, y, 5)) ground = y;
  host->connection().send(MovePlayer{{5.5f, ground + 1.0f, 8.5f}});
  host->connection().send(BreakBlock{{5, ground, 5}});
  got.clear();
  REQUIRE(until(*guest, got, has<BlockChanged>));
  auto edits = guest->world().editList();
  CHECK(std::find(edits.begin(), edits.end(), BlockEdit{{5, ground, 5}, BlockType::AIR}) !=
        edits.end());
}

TEST_CASE_FIXTURE(Sessions, "the guest's clock follows the host's") {
  auto guest = join("Bea");
  std::vector<Event> got;
  REQUIRE(until(*guest, got, has<Joined>));
  host->connection().send(SetTimeOfDay{1000.0f});
  got.clear();
  REQUIRE(until(*guest, got, has<TimeSync>));
  CHECK(guest->clock().time_of_day == doctest::Approx(host->clock().time_of_day).epsilon(0.02));
}

TEST_CASE_FIXTURE(Sessions, "ending a hosted session sends guests home") {
  auto guest = join("Bea");
  std::vector<Event> got;
  REQUIRE(until(*guest, got, has<Joined>));
  host->end();
  got.clear();
  for (int i = 0; i < 300 && !has<Disconnected>(got); ++i) {
    guest->poll(got);
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  CHECK(has<Disconnected>(got));
  CHECK(std::filesystem::exists(dir.path / "players" / "Bea.dat"));
  CHECK(std::filesystem::exists(dir.path / "level.dat"));
}

TEST_CASE("joining nobody fails with a reason") {
  std::string error;
  net::Socket probe = net::listenTcp(0, error);
  uint16_t port = net::localPort(probe);
  probe.close();
  Session::Options o;
  o.name = "Bea";
  o.port = port;
  CHECK_FALSE(Session::join("127.0.0.1", o, error));
  CHECK_FALSE(error.empty());
}
