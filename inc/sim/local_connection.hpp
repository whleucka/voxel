#pragma once

#include "sim/connection.hpp"
#include <deque>

// Single-player link: both ends live in the same process and are only touched
// from the main thread, so plain FIFO queues are enough.
class LocalConnection : public Connection, public ClientLink {
public:
  // Connection (client side)
  void send(Command cmd) override;
  void poll(std::vector<Event> &out) override;

  // ClientLink (Game side)
  void drain(std::vector<Command> &out) override;
  void deliver(const Event &ev) override;

private:
  std::deque<Command> commands;
  std::deque<Event> events;
};
