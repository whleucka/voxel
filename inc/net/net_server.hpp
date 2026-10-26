#pragma once

#include "net/socket.hpp"
#include "sim/connection.hpp"
#include <deque>
#include <memory>
#include <string>
#include <vector>

class Game;

// The host's side of multiplayer: accepts guests and attaches each to the
// Game as a Guest ClientLink. Polled from the main thread; never blocks.
class NetServer {
public:
  explicit NetServer(Game &game);
  ~NetServer();

  // Starts listening; port 0 picks a free port (see port()).
  bool listen(uint16_t port, std::string &error);
  uint16_t port() const { return port_; }
  size_t guests() const { return remotes.size(); }

  // Accepts newcomers, reads their commands (for the next Game::pump) and
  // sends what the Game delivered. Guests whose link failed leave the Game.
  void poll();
  // Sends what the Game delivered since the last poll, without reading.
  void flush();
  // Tells every guest why and closes everything.
  void shutdown(const std::string &reason);

private:
  class Remote;
  Game &game;
  net::Socket listener;
  uint16_t port_ = 0;
  std::vector<std::unique_ptr<Remote>> remotes;
};
