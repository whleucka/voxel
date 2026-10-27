#pragma once

#include "core/game_mode.hpp"
#include "net/socket.hpp"
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

// Finding games on the home network: a host broadcasts a small UDP beacon
// every second; the join screen listens and lists what it hears.
namespace discovery {

constexpr uint16_t kPort = 47801;
constexpr double kBeaconInterval = 1.0; // seconds
constexpr double kExpiry = 3.0;         // a game unheard for this long is gone

struct Beacon {
  uint16_t version = 0;   // wire::kProtocolVersion of the host
  std::string world;
  std::string host;       // the host player's name
  uint8_t players = 0;
  uint8_t max_players = 0;
  uint16_t port = 0;      // the host's TCP port
  GameMode mode = GameMode::Survival; // from version 2
};

std::vector<uint8_t> encode(const Beacon &b);
std::optional<Beacon> decode(std::span<const uint8_t> bytes);

// The host's side: call tick() every frame; it sends once a second.
class BeaconSender {
public:
  bool start(std::string &error, std::string target = "255.255.255.255",
             uint16_t target_port = kPort);
  void tick(double now, const Beacon &beacon);

private:
  net::Socket sock;
  std::string target;
  uint16_t target_port = kPort;
  double next_send = 0.0;
};

struct FoundGame {
  std::string address; // where the beacon came from
  Beacon beacon;
  double last_seen = 0.0;
};

// The join screen's list of games.
class LanBrowser {
public:
  bool start(std::string &error, uint16_t port = kPort);
  uint16_t port() const { return net::localPort(sock); }
  // Reads beacons that arrived and drops games not heard for kExpiry.
  void poll(double now);
  const std::vector<FoundGame> &games() const { return found; }

  // The pure part of poll(), for tests.
  void heard(const std::string &address, const Beacon &b, double now);
  void expire(double now);

private:
  net::Socket sock;
  std::vector<FoundGame> found; // in the order first heard
};

} // namespace discovery
