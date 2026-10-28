#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <glm/glm.hpp>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// Chat rules and the client's chat log. Pure logic: the Game cleans and
// rate-limits what players send, and the HUD asks the log what to draw.

constexpr size_t kMaxChatChars = 100;   // UTF-8 characters, not bytes
constexpr size_t kChatHistory = 50;     // kept by the host and by each client
constexpr size_t kChatOnJoin = 20;      // sent to a player who joins
constexpr size_t kChatVisibleLines = 6; // shown over the game while closed
constexpr double kChatLineSeconds = 10.0;
constexpr double kChatFadeSeconds = 1.0;

// Trimmed, control characters turned into spaces, cut to kMaxChatChars.
// Nothing when what's left is empty.
std::optional<std::string> cleanChatText(std::string_view text);

// Per-player spam limit: at most kBurst messages in kWindowMs, and never the
// same text again within kRepeatMs.
class ChatLimiter {
public:
  static constexpr int kBurst = 5;
  static constexpr int64_t kWindowMs = 5000;
  static constexpr int64_t kRepeatMs = 3000;
  enum class Verdict { Ok, TooFast, Repeat };

  // Records the message when it's allowed.
  Verdict check(const std::string &text, int64_t now_ms);

private:
  std::deque<int64_t> sent_ms;
  std::string last_text;
  int64_t last_ms = 0;
};

// "7:42 pm" for minutes since midnight.
std::string formatClock(uint16_t minute);

// A bright colour for a player's name, the same on every screen.
glm::vec3 chatNameColor(uint32_t player_id);

struct ChatLine {
  uint16_t minute = 0;
  uint32_t from = 0;  // 0: game news
  std::string name;
  std::string text;
  double arrived = 0; // client time, seconds
};

// The last kChatHistory lines, oldest first.
class ChatLog {
public:
  void add(ChatLine line);
  const std::deque<ChatLine> &lines() const { return lines_; }
  void clear() { lines_.clear(); }

  struct Shown {
    const ChatLine *line;
    float alpha; // 1 until the last second, then down to 0
  };
  // The newest lines still on screen at `now` while chat is closed, oldest
  // first, at most kChatVisibleLines.
  std::vector<Shown> visible(double now) const;

private:
  std::deque<ChatLine> lines_;
};
