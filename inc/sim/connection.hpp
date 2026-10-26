#pragma once

#include "sim/protocol.hpp"
#include <vector>

// The client's end of a link to a Game.
class Connection {
public:
  virtual ~Connection() = default;
  virtual void send(Command cmd) = 0;
  // Appends every event received since the last poll, in order.
  virtual void poll(std::vector<Event> &out) = 0;
};

// The Game's end of a link to one client.
class ClientLink {
public:
  virtual ~ClientLink() = default;
  // Appends every command received since the last drain, in order.
  virtual void drain(std::vector<Command> &out) = 0;
  virtual void deliver(const Event &ev) = 0;
};
