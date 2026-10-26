#include "net/net_server.hpp"
#include "net/wire.hpp"
#include "sim/game.hpp"
#include <iostream>

// One guest: a framed stream that is also the Game's link to that guest.
class NetServer::Remote : public ClientLink {
public:
  explicit Remote(net::Socket s) : stream(std::move(s)) { stream.send(wire::hello()); }

  void drain(std::vector<Command> &out) override {
    for (Command &c : commands) out.push_back(std::move(c));
    commands.clear();
  }
  void deliver(const Event &ev) override {
    stream.send(wire::encode(ev));
    // A refused join gets its reason, then the door.
    if (std::holds_alternative<JoinRefused>(ev)) closing = true;
  }

  // Reads frames: the first must be a matching hello, the rest commands.
  void receive() {
    std::vector<std::vector<uint8_t>> frames;
    stream.receive(frames);
    for (const auto &f : frames) {
      if (!greeted) {
        if (wire::readHello(f) != wire::kProtocolVersion) return stream.close("Different version");
        greeted = true;
        continue;
      }
      auto cmd = wire::decodeCommand(f);
      if (!cmd) return stream.close("Bad data");
      commands.push_back(std::move(*cmd));
    }
  }

  net::Stream stream;
  std::vector<Command> commands;
  bool greeted = false;
  bool closing = false;
};

NetServer::NetServer(Game &game) : game(game) {}
NetServer::~NetServer() { shutdown("Host left the game"); }

bool NetServer::listen(uint16_t port, std::string &error) {
  listener = net::listenTcp(port, error);
  if (!listener.valid()) return false;
  port_ = net::localPort(listener);
  return true;
}

void NetServer::poll() {
  while (listener.valid()) {
    net::Socket s = net::acceptTcp(listener);
    if (!s.valid()) break;
    remotes.push_back(std::make_unique<Remote>(std::move(s)));
    game.addClient(*remotes.back(), Game::Role::Guest);
  }
  for (auto &r : remotes) r->receive();
  flush();
}

void NetServer::flush() {
  for (auto &r : remotes) {
    r->stream.flush();
    if (r->closing && !r->stream.pending()) r->stream.close("Join refused");
  }
  // Gone guests leave the Game before their link is destroyed.
  std::erase_if(remotes, [&](const std::unique_ptr<Remote> &r) {
    if (r->stream.open()) return false;
    std::cout << "[net] guest left: " << r->stream.error() << "\n";
    game.removeClient(*r);
    return true;
  });
}

void NetServer::shutdown(const std::string &reason) {
  for (auto &r : remotes) {
    r->stream.send(wire::encode(Event{Disconnected{reason}}));
    r->stream.flush();
    game.removeClient(*r);
  }
  remotes.clear();
  listener.close();
}
