#include "core/seed.hpp"
#include <cctype>
#include <charconv>
#include <ctime>
#include <functional>
#include <string>

uint32_t resolveSeed(std::string_view text) {
  while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front())))
    text.remove_prefix(1);
  while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back())))
    text.remove_suffix(1);
  if (text.empty()) return static_cast<uint32_t>(std::time(nullptr));

  // A number (optionally signed) wraps into 32 bits, as it always has; one
  // too long to parse is treated as text.
  std::string_view digits = text;
  bool negative = false;
  if (digits.front() == '-' || digits.front() == '+') {
    negative = digits.front() == '-';
    digits.remove_prefix(1);
  }
  unsigned long long value = 0;
  auto [end, ec] = std::from_chars(digits.data(), digits.data() + digits.size(), value);
  if (!digits.empty() && ec == std::errc() && end == digits.data() + digits.size())
    return static_cast<uint32_t>(negative ? 0ull - value : value);

  return static_cast<uint32_t>(std::hash<std::string_view>{}(text));
}

glm::vec2 noiseOffsetFor(uint32_t seed) {
  return {static_cast<float>(seed % 5000), static_cast<float>((seed / 5000) % 5000)};
}
