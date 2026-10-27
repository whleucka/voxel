#include "net/discovery.hpp"
#include <algorithm>
#include <cstring>

namespace discovery {
namespace {

constexpr uint8_t kMagic[4] = {'V', 'X', 'L', 'B'};

void putString(std::vector<uint8_t> &out, const std::string &s) {
  uint8_t n = static_cast<uint8_t>(std::min<size_t>(s.size(), 255));
  out.push_back(n);
  out.insert(out.end(), s.begin(), s.begin() + n);
}

bool getString(std::span<const uint8_t> in, size_t &pos, std::string &s) {
  if (pos >= in.size()) return false;
  size_t n = in[pos++];
  if (in.size() - pos < n) return false;
  s.assign(reinterpret_cast<const char *>(in.data() + pos), n);
  pos += n;
  return true;
}

} // namespace

std::vector<uint8_t> encode(const Beacon &b) {
  std::vector<uint8_t> out(kMagic, kMagic + 4);
  out.push_back(static_cast<uint8_t>(b.version)), out.push_back(static_cast<uint8_t>(b.version >> 8));
  putString(out, b.world);
  putString(out, b.host);
  out.push_back(b.players);
  out.push_back(b.max_players);
  out.push_back(static_cast<uint8_t>(b.port)), out.push_back(static_cast<uint8_t>(b.port >> 8));
  out.push_back(static_cast<uint8_t>(b.mode));
  return out;
}

std::optional<Beacon> decode(std::span<const uint8_t> in) {
  if (in.size() < 6 || std::memcmp(in.data(), kMagic, 4) != 0) return std::nullopt;
  Beacon b;
  b.version = static_cast<uint16_t>(in[4] | in[5] << 8);
  size_t pos = 6;
  if (!getString(in, pos, b.world) || !getString(in, pos, b.host)) return std::nullopt;
  // Version 1 hosts send no mode byte; they still list, as a different version.
  size_t tail = b.version < 2 ? 4 : 5;
  if (in.size() - pos != tail) return std::nullopt;
  b.players = in[pos];
  b.max_players = in[pos + 1];
  b.port = static_cast<uint16_t>(in[pos + 2] | in[pos + 3] << 8);
  if (tail == 5 && in[pos + 4] == static_cast<uint8_t>(GameMode::Creative))
    b.mode = GameMode::Creative;
  return b;
}

bool BeaconSender::start(std::string &error, std::string to, uint16_t to_port) {
  sock = net::openUdp(0, error);
  target = std::move(to);
  target_port = to_port;
  next_send = 0.0;
  return sock.valid();
}

void BeaconSender::tick(double now, const Beacon &beacon) {
  if (!sock.valid() || now < next_send) return;
  next_send = now + kBeaconInterval;
  net::sendUdp(sock, target, target_port, encode(beacon));
}

bool LanBrowser::start(std::string &error, uint16_t port) {
  sock = net::openUdp(port, error);
  found.clear();
  return sock.valid();
}

void LanBrowser::poll(double now) {
  std::vector<uint8_t> bytes;
  std::string from;
  while (sock.valid() && net::recvUdp(sock, bytes, from))
    if (auto b = decode(bytes)) heard(from, *b, now);
  expire(now);
}

void LanBrowser::heard(const std::string &address, const Beacon &b, double now) {
  auto it = std::find_if(found.begin(), found.end(), [&](const FoundGame &g) {
    return g.address == address && g.beacon.port == b.port;
  });
  if (it == found.end()) found.push_back({address, b, now});
  else it->beacon = b, it->last_seen = now;
}

void LanBrowser::expire(double now) {
  std::erase_if(found, [&](const FoundGame &g) { return now - g.last_seen > kExpiry; });
}

} // namespace discovery
