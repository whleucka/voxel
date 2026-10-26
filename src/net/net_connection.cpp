#include "net/net_connection.hpp"
#include "net/wire.hpp"

bool NetConnection::connect(const std::string &host, uint16_t port, std::string &error,
                            int timeout_ms) {
  net::Socket s = net::connectTcp(host, port, timeout_ms, error);
  if (!s.valid()) return false;
  stream = std::make_unique<net::Stream>(std::move(s));
  stream->send(wire::hello());
  stream->flush();
  greeted = false;
  reported = false;
  return true;
}

void NetConnection::send(Command cmd) {
  if (open()) stream->send(wire::encode(cmd));
}

void NetConnection::fail(std::vector<Event> &out, std::string reason) {
  if (stream) stream->close(reason);
  if (!reported) out.push_back(Disconnected{std::move(reason)});
  reported = true;
}

void NetConnection::poll(std::vector<Event> &out) {
  if (!stream || reported) return;
  stream->flush();
  std::vector<std::vector<uint8_t>> frames;
  stream->receive(frames);
  for (const auto &f : frames) {
    if (!greeted) {
      if (wire::readHello(f) != wire::kProtocolVersion) return fail(out, "Different game version");
      greeted = true;
      continue;
    }
    auto ev = wire::decodeEvent(f);
    if (!ev) return fail(out, "Bad data from the host");
    // The host's goodbye (it quit, or refused the join) ends the link; the
    // event itself carries the reason.
    bool goodbye = std::holds_alternative<Disconnected>(*ev) ||
                   std::holds_alternative<JoinRefused>(*ev);
    out.push_back(std::move(*ev));
    if (goodbye) {
      stream->close("Closed by host");
      reported = true;
      return;
    }
  }
  if (!stream->open()) fail(out, greeted ? "Lost connection to the host" : "The host didn't answer");
}
