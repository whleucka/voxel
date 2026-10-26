#pragma once

#include "net/socket.hpp"
#include "sim/connection.hpp"
#include <memory>
#include <string>

// A guest's link to a host over TCP. Commands are framed and sent on poll();
// events arrive through poll() too. Losing the host (or a version mismatch)
// shows up as a Disconnected event, after which the connection is closed.
class NetConnection : public Connection {
public:
  // Connects (waiting at most timeout_ms) and says hello.
  bool connect(const std::string &host, uint16_t port, std::string &error,
               int timeout_ms = 3000);
  bool open() const { return stream && stream->open(); }

  void send(Command cmd) override;
  void poll(std::vector<Event> &out) override;

private:
  void fail(std::vector<Event> &out, std::string reason);

  std::unique_ptr<net::Stream> stream;
  bool greeted = false;   // the host's hello arrived and matched
  bool reported = false;  // Disconnected already handed out
};
