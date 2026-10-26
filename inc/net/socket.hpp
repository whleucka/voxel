#pragma once

#include "net/wire.hpp"
#include <cstdint>
#include <span>
#include <string>
#include <vector>

// Thin RAII wrappers over POSIX sockets. Everything is non-blocking except
// connectTcp, which waits a few seconds at most (it runs from the menu).
namespace net {

class Socket {
public:
  Socket() = default;
  explicit Socket(int fd) : fd_(fd) {}
  Socket(Socket &&o) noexcept : fd_(o.release()) {}
  Socket &operator=(Socket &&o) noexcept;
  Socket(const Socket &) = delete;
  Socket &operator=(const Socket &) = delete;
  ~Socket() { close(); }

  bool valid() const { return fd_ >= 0; }
  int fd() const { return fd_; }
  int release() {
    int f = fd_;
    fd_ = -1;
    return f;
  }
  void close();

private:
  int fd_ = -1;
};

// Listens on every interface; port 0 picks a free one (see localPort).
Socket listenTcp(uint16_t port, std::string &error);
// A pending connection, or an invalid socket if there is none.
Socket acceptTcp(const Socket &listener);
Socket connectTcp(const std::string &host, uint16_t port, int timeout_ms, std::string &error);
uint16_t localPort(const Socket &s);

// A UDP socket bound to `port` (0: any) that may send broadcasts.
Socket openUdp(uint16_t port, std::string &error);
bool sendUdp(const Socket &s, const std::string &host, uint16_t port,
             std::span<const uint8_t> bytes);
// One datagram if there is one; `from` gets the sender's address.
bool recvUdp(const Socket &s, std::vector<uint8_t> &out, std::string &from);

// The machine's LAN addresses (IPv4, not loopback), for showing the host's
// address to type into Join.
std::vector<std::string> lanAddresses();

// A connected TCP socket carrying frames. Writes are buffered and flushed
// without blocking; reads are split into payloads by a FrameReader.
class Stream {
public:
  static constexpr size_t kMaxPending = 8 * 1024 * 1024; // a guest this far behind is dropped

  explicit Stream(Socket s) : sock(std::move(s)) {}
  bool open() const { return sock.valid(); }
  const std::string &error() const { return error_; }

  void send(std::span<const uint8_t> payload);
  // Writes as much buffered data as the socket takes now.
  void flush();
  // Reads what has arrived; complete payloads go to out.
  void receive(std::vector<std::vector<uint8_t>> &out);
  void close(std::string reason);
  bool pending() const { return sent < outgoing.size(); }

private:
  Socket sock;
  wire::FrameReader reader;
  std::vector<uint8_t> outgoing;
  size_t sent = 0;
  std::string error_;
};

} // namespace net
