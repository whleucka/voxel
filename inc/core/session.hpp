#pragma once

#include "net/discovery.hpp"
#include "net/net_connection.hpp"
#include "net/net_server.hpp"
#include "sim/game.hpp"
#include "sim/local_connection.hpp"
#include "world/game_clock.hpp"
#include "world/world.hpp"
#include <memory>
#include <string>

// One game being played, from the client's side: which world it draws, which
// link it talks through, and the plumbing each mode needs.
//
//   Single: an in-process Game, nothing on the network.
//   Host:   the same, plus a NetServer for guests and a LAN beacon.
//   Guest:  no Game; its own World (from the host's Welcome) and clock.
//
// GL-free: the Engine calls world().init() once the session exists.
class Session {
public:
  enum class Mode { Single, Host, Guest };

  struct Options {
    std::string name;        // the player's name
    std::string save_dir;    // single/host: the world to load or create
    std::string world_name;  // host: shown in the beacon
    uint16_t port = 0;       // host: TCP port (0 = any); guest: the host's
    bool beacon = true;      // host: announce on the LAN
  };

  // Single player or host. Nothing on failure (error says why).
  static std::unique_ptr<Session> local(Mode mode, const Options &opts, std::string &error);
  // Joins a host. Nothing if it can't connect (error says why).
  static std::unique_ptr<Session> join(const std::string &address, const Options &opts,
                                       std::string &error);
  ~Session();

  Mode mode() const { return mode_; }
  bool isGuest() const { return mode_ == Mode::Guest; }
  World &world() { return game_ ? game_->world() : *client_world; }
  const GameClock &clock() const { return game_ ? game_->clock() : client_clock; }
  Connection &connection();
  Game *game() { return game_.get(); }
  NetServer *server() { return server_.get(); }

  // One frame: the network, then the Game's commands and ticks (or, for a
  // guest, its clock). `now` is wall time in seconds (beacon pacing).
  void frame(float dt, double now);
  // Where the local player is: a guest's world streams chunks around it.
  void follow(glm::vec3 position);
  // Events for the client. A guest's world-level events (Welcome, TimeSync,
  // BlockChanged) are applied to its world and clock here, then passed on.
  void poll(std::vector<Event> &out);
  // Saves (single/host) and says goodbye to guests (host). Idempotent; the
  // destructor calls it.
  void end();

private:
  explicit Session(Mode mode) : mode_(mode) {}

  Mode mode_;
  bool ended = false;
  Options opts;

  // Order matters for destruction: the server's goodbye still reaches the
  // local link, and both go before the Game they're attached to.
  std::unique_ptr<Game> game_;
  LocalConnection local_link;
  std::unique_ptr<NetServer> server_;
  discovery::BeaconSender beacon;

  std::unique_ptr<World> client_world;
  std::unique_ptr<NetConnection> remote;
  GameClock client_clock;
  // A guest borrows the host's seed; the player's own is put back after.
  uint32_t saved_seed = 0;
  glm::vec2 saved_offset{0.0f};
  uint8_t saved_worldgen = 0;
};
