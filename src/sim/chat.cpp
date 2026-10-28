#include "sim/chat.hpp"
#include <algorithm>

namespace {

bool isSpace(char c) { return c == ' '; }
bool isContinuation(unsigned char c) { return (c & 0xC0) == 0x80; }

std::string_view trim(std::string_view s) {
  while (!s.empty() && isSpace(s.front())) s.remove_prefix(1);
  while (!s.empty() && isSpace(s.back())) s.remove_suffix(1);
  return s;
}

} // namespace

std::optional<std::string> cleanChatText(std::string_view text) {
  std::string out(text);
  for (char &c : out) {
    unsigned char u = static_cast<unsigned char>(c);
    if (u < 0x20 || u == 0x7F) c = ' ';
  }
  std::string_view t = trim(out);

  // Cut after kMaxChatChars characters, never inside one.
  size_t chars = 0, end = 0;
  while (end < t.size()) {
    if (!isContinuation(static_cast<unsigned char>(t[end])) && chars++ == kMaxChatChars) break;
    ++end;
  }
  t = trim(t.substr(0, end));
  if (t.empty()) return std::nullopt;
  return std::string(t);
}

ChatLimiter::Verdict ChatLimiter::check(const std::string &text, int64_t now_ms) {
  while (!sent_ms.empty() && now_ms - sent_ms.front() >= kWindowMs) sent_ms.pop_front();
  if (!last_text.empty() && text == last_text && now_ms - last_ms < kRepeatMs)
    return Verdict::Repeat;
  if (static_cast<int>(sent_ms.size()) >= kBurst) return Verdict::TooFast;
  sent_ms.push_back(now_ms);
  last_text = text;
  last_ms = now_ms;
  return Verdict::Ok;
}

std::string formatClock(uint16_t minute) {
  int h = (minute / 60) % 24, m = minute % 60;
  int h12 = h % 12 == 0 ? 12 : h % 12;
  std::string mm = (m < 10 ? "0" : "") + std::to_string(m);
  return std::to_string(h12) + ":" + mm + (h < 12 ? " am" : " pm");
}

glm::vec3 chatNameColor(uint32_t player_id) {
  static const glm::vec3 kPalette[] = {
      {1.00f, 0.55f, 0.55f}, {0.55f, 0.85f, 1.00f}, {0.65f, 1.00f, 0.55f},
      {1.00f, 0.85f, 0.40f}, {0.85f, 0.65f, 1.00f}, {1.00f, 0.65f, 0.90f},
      {0.50f, 1.00f, 0.85f}, {1.00f, 0.70f, 0.45f},
  };
  return kPalette[player_id % std::size(kPalette)];
}

void ChatLog::add(ChatLine line) {
  lines_.push_back(std::move(line));
  while (lines_.size() > kChatHistory) lines_.pop_front();
}

std::vector<ChatLog::Shown> ChatLog::visible(double now) const {
  std::vector<Shown> out;
  for (auto it = lines_.rbegin(); it != lines_.rend() && out.size() < kChatVisibleLines; ++it) {
    double age = now - it->arrived;
    if (age >= kChatLineSeconds) break; // older lines arrived earlier still
    double left = kChatLineSeconds - age;
    float alpha = left >= kChatFadeSeconds ? 1.0f : static_cast<float>(left / kChatFadeSeconds);
    out.push_back({&*it, alpha});
  }
  std::reverse(out.begin(), out.end());
  return out;
}
