#pragma once

#include "sim/protocol.hpp"
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

// The bytes that go over the network: a length-prefixed frame per message,
// each holding one Command (guest -> host) or Event (host -> guest). Pure, so
// it is tested without sockets.
//
// Frame:   u32 payload length (little-endian), then the payload.
// Payload: u8 variant index, then the alternative's fields in order. Numbers
//          are little-endian; floats must be finite; enums must be known
//          values; strings are u16 length + bytes.
namespace wire {

constexpr uint16_t kProtocolVersion = 3; // 3: Welcome carries the worldgen version
constexpr uint32_t kMaxFrame = 4 * 1024 * 1024;

std::vector<uint8_t> encode(const Command &cmd);
std::vector<uint8_t> encode(const Event &ev);
// Nothing if the payload is malformed: unknown index, short, trailing bytes,
// a non-finite float or an unknown enum value.
std::optional<Command> decodeCommand(std::span<const uint8_t> payload);
std::optional<Event> decodeEvent(std::span<const uint8_t> payload);

// The first frame each side sends: magic "VOXL" and the protocol version.
std::vector<uint8_t> hello();
// The version in a hello frame, or nothing if it isn't one.
std::optional<uint16_t> readHello(std::span<const uint8_t> payload);

// Prefixes a payload with its length.
std::vector<uint8_t> frame(std::span<const uint8_t> payload);

// Splits a byte stream back into payloads, however it arrives.
class FrameReader {
public:
  void feed(std::span<const uint8_t> bytes);
  // The next complete payload, if one has arrived.
  std::optional<std::vector<uint8_t>> next();
  // A frame claimed more than kMaxFrame bytes: the stream can't be trusted.
  bool bad() const { return bad_; }

private:
  std::vector<uint8_t> buf;
  size_t start = 0;
  bool bad_ = false;
};

} // namespace wire
