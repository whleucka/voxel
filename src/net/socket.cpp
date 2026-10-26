#include "net/socket.hpp"
#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

namespace net {
namespace {

bool setNonBlocking(int fd) {
  int flags = fcntl(fd, F_GETFL, 0);
  return flags >= 0 && fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0;
}

void setNoDelay(int fd) {
  int one = 1;
  setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof one);
}

bool resolve(const std::string &host, uint16_t port, sockaddr_in &out) {
  addrinfo hints{};
  hints.ai_family = AF_INET;
  addrinfo *res = nullptr;
  if (getaddrinfo(host.c_str(), nullptr, &hints, &res) != 0 || !res) return false;
  out = *reinterpret_cast<sockaddr_in *>(res->ai_addr);
  out.sin_port = htons(port);
  freeaddrinfo(res);
  return true;
}

std::string lastError(const char *what) { return std::string(what) + ": " + std::strerror(errno); }

} // namespace

Socket &Socket::operator=(Socket &&o) noexcept {
  if (this != &o) {
    close();
    fd_ = o.release();
  }
  return *this;
}

void Socket::close() {
  if (fd_ >= 0) ::close(fd_);
  fd_ = -1;
}

Socket listenTcp(uint16_t port, std::string &error) {
  Socket s(::socket(AF_INET, SOCK_STREAM, 0));
  if (!s.valid()) return error = lastError("socket"), Socket{};
  int one = 1;
  setsockopt(s.fd(), SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  addr.sin_port = htons(port);
  if (bind(s.fd(), reinterpret_cast<sockaddr *>(&addr), sizeof addr) != 0)
    return error = lastError("bind"), Socket{};
  if (::listen(s.fd(), 8) != 0) return error = lastError("listen"), Socket{};
  if (!setNonBlocking(s.fd())) return error = lastError("fcntl"), Socket{};
  return s;
}

Socket acceptTcp(const Socket &listener) {
  int fd = ::accept(listener.fd(), nullptr, nullptr);
  if (fd < 0) return Socket{};
  Socket s(fd);
  if (!setNonBlocking(fd)) return Socket{};
  setNoDelay(fd);
  return s;
}

Socket connectTcp(const std::string &host, uint16_t port, int timeout_ms, std::string &error) {
  sockaddr_in addr{};
  if (!resolve(host, port, addr)) return error = "Can't find '" + host + "'", Socket{};
  Socket s(::socket(AF_INET, SOCK_STREAM, 0));
  if (!s.valid() || !setNonBlocking(s.fd())) return error = lastError("socket"), Socket{};
  if (::connect(s.fd(), reinterpret_cast<sockaddr *>(&addr), sizeof addr) != 0) {
    if (errno != EINPROGRESS) return error = "Can't connect: " + std::string(std::strerror(errno)), Socket{};
    pollfd p{s.fd(), POLLOUT, 0};
    if (::poll(&p, 1, timeout_ms) != 1) return error = "No answer from " + host, Socket{};
    int err = 0;
    socklen_t len = sizeof err;
    getsockopt(s.fd(), SOL_SOCKET, SO_ERROR, &err, &len);
    if (err != 0) return error = "Can't connect: " + std::string(std::strerror(err)), Socket{};
  }
  setNoDelay(s.fd());
  return s;
}

uint16_t localPort(const Socket &s) {
  sockaddr_in addr{};
  socklen_t len = sizeof addr;
  if (getsockname(s.fd(), reinterpret_cast<sockaddr *>(&addr), &len) != 0) return 0;
  return ntohs(addr.sin_port);
}

Socket openUdp(uint16_t port, std::string &error) {
  Socket s(::socket(AF_INET, SOCK_DGRAM, 0));
  if (!s.valid()) return error = lastError("socket"), Socket{};
  int one = 1;
  setsockopt(s.fd(), SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
  setsockopt(s.fd(), SOL_SOCKET, SO_BROADCAST, &one, sizeof one);
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  addr.sin_port = htons(port);
  if (bind(s.fd(), reinterpret_cast<sockaddr *>(&addr), sizeof addr) != 0)
    return error = lastError("bind"), Socket{};
  if (!setNonBlocking(s.fd())) return error = lastError("fcntl"), Socket{};
  return s;
}

bool sendUdp(const Socket &s, const std::string &host, uint16_t port,
             std::span<const uint8_t> bytes) {
  sockaddr_in addr{};
  if (!resolve(host, port, addr)) return false;
  return ::sendto(s.fd(), bytes.data(), bytes.size(), 0, reinterpret_cast<sockaddr *>(&addr),
                  sizeof addr) == static_cast<ssize_t>(bytes.size());
}

bool recvUdp(const Socket &s, std::vector<uint8_t> &out, std::string &from) {
  uint8_t buf[2048];
  sockaddr_in addr{};
  socklen_t len = sizeof addr;
  ssize_t n = ::recvfrom(s.fd(), buf, sizeof buf, 0, reinterpret_cast<sockaddr *>(&addr), &len);
  if (n < 0) return false;
  out.assign(buf, buf + n);
  char text[INET_ADDRSTRLEN] = {};
  inet_ntop(AF_INET, &addr.sin_addr, text, sizeof text);
  from = text;
  return true;
}

std::vector<std::string> lanAddresses() {
  std::vector<std::string> out;
  ifaddrs *list = nullptr;
  if (getifaddrs(&list) != 0) return out;
  for (ifaddrs *i = list; i; i = i->ifa_next) {
    if (!i->ifa_addr || i->ifa_addr->sa_family != AF_INET) continue;
    auto *a = reinterpret_cast<sockaddr_in *>(i->ifa_addr);
    if (ntohl(a->sin_addr.s_addr) >> 24 == 127) continue; // loopback
    char text[INET_ADDRSTRLEN] = {};
    inet_ntop(AF_INET, &a->sin_addr, text, sizeof text);
    out.push_back(text);
  }
  freeifaddrs(list);
  return out;
}

// ─── Stream ────────────────────────────────────────────────────────────

void Stream::send(std::span<const uint8_t> payload) {
  if (!open()) return;
  auto f = wire::frame(payload);
  outgoing.insert(outgoing.end(), f.begin(), f.end());
  if (outgoing.size() - sent > kMaxPending) close("Too far behind");
}

void Stream::flush() {
  while (open() && sent < outgoing.size()) {
    ssize_t n = ::send(sock.fd(), outgoing.data() + sent, outgoing.size() - sent, MSG_NOSIGNAL);
    if (n < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) break;
      return close("Connection lost");
    }
    sent += static_cast<size_t>(n);
  }
  if (sent == outgoing.size()) {
    outgoing.clear();
    sent = 0;
  }
}

void Stream::receive(std::vector<std::vector<uint8_t>> &out) {
  uint8_t buf[64 * 1024];
  while (open()) {
    ssize_t n = ::recv(sock.fd(), buf, sizeof buf, 0);
    if (n == 0) {
      close("Connection closed");
      break;
    }
    if (n < 0) {
      if (errno != EAGAIN && errno != EWOULDBLOCK) close("Connection lost");
      break;
    }
    reader.feed(std::span(buf, static_cast<size_t>(n)));
  }
  // Frames that arrived before a close still count (a host's goodbye).
  while (auto p = reader.next()) out.push_back(std::move(*p));
  if (reader.bad()) close("Bad data");
}

void Stream::close(std::string reason) {
  if (!open()) return;
  if (error_.empty()) error_ = std::move(reason);
  sock.close();
}

} // namespace net
