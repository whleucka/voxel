#include "sim/local_connection.hpp"

void LocalConnection::send(Command cmd) { commands.push_back(std::move(cmd)); }

void LocalConnection::poll(std::vector<Event> &out) {
  out.insert(out.end(), std::make_move_iterator(events.begin()),
             std::make_move_iterator(events.end()));
  events.clear();
}

void LocalConnection::drain(std::vector<Command> &out) {
  out.insert(out.end(), std::make_move_iterator(commands.begin()),
             std::make_move_iterator(commands.end()));
  commands.clear();
}

void LocalConnection::deliver(const Event &ev) { events.push_back(ev); }
