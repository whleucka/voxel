#include "doctest/doctest.h"
#include "sim/chat.hpp"

TEST_CASE("chat text is trimmed, control characters become spaces") {
  CHECK(cleanChatText("  hi there  ") == "hi there");
  CHECK(cleanChatText("a\tb\nc") == "a b c");
  CHECK(cleanChatText("\n\n  ") == std::nullopt);
  CHECK(cleanChatText("") == std::nullopt);
}

TEST_CASE("chat text is cut at 100 characters, never inside one") {
  std::string long_text(150, 'x');
  CHECK(cleanChatText(long_text)->size() == kMaxChatChars);

  std::string hearts;
  for (int i = 0; i < 120; ++i) hearts += "\xE2\x99\xA5"; // ♥, 3 bytes
  std::string cut = *cleanChatText(hearts);
  CHECK(cut.size() == kMaxChatChars * 3);

  // Space at the cut is trimmed off too.
  std::string spaced = std::string(99, 'a') + "  b";
  CHECK(*cleanChatText(spaced) == std::string(99, 'a'));
}

TEST_CASE("the limiter allows 5 messages in 5 seconds") {
  ChatLimiter lim;
  for (int i = 0; i < 5; ++i)
    CHECK(lim.check("msg " + std::to_string(i), i * 100) == ChatLimiter::Verdict::Ok);
  CHECK(lim.check("one more", 600) == ChatLimiter::Verdict::TooFast);
  CHECK(lim.check("later", 5000) == ChatLimiter::Verdict::Ok); // the first has aged out
}

TEST_CASE("the same text again within 3 seconds is dropped") {
  ChatLimiter lim;
  CHECK(lim.check("hi", 0) == ChatLimiter::Verdict::Ok);
  CHECK(lim.check("hi", 2999) == ChatLimiter::Verdict::Repeat);
  CHECK(lim.check("hi", 3000) == ChatLimiter::Verdict::Ok);
  CHECK(lim.check("hello", 3001) == ChatLimiter::Verdict::Ok);
}

TEST_CASE("clock times are 12-hour with am and pm") {
  CHECK(formatClock(0) == "12:00 am");
  CHECK(formatClock(7 * 60 + 5) == "7:05 am");
  CHECK(formatClock(12 * 60) == "12:00 pm");
  CHECK(formatClock(19 * 60 + 42) == "7:42 pm");
  CHECK(formatClock(23 * 60 + 59) == "11:59 pm");
}

TEST_CASE("each player keeps one name colour") {
  CHECK(chatNameColor(3) == chatNameColor(3));
  CHECK(chatNameColor(1) != chatNameColor(2));
}

TEST_CASE("the chat log keeps the last 50 lines") {
  ChatLog log;
  for (int i = 0; i < 60; ++i) log.add({0, 1, "A", std::to_string(i), 0.0});
  REQUIRE(log.lines().size() == kChatHistory);
  CHECK(log.lines().front().text == "10");
  CHECK(log.lines().back().text == "59");
}

TEST_CASE("at most 6 lines show, for 10 seconds, fading in the last one") {
  ChatLog log;
  for (int i = 0; i < 8; ++i) log.add({0, 1, "A", std::to_string(i), i * 1.0});
  auto shown = log.visible(8.0);
  REQUIRE(shown.size() == kChatVisibleLines);
  CHECK(shown.front().line->text == "2");
  CHECK(shown.back().line->text == "7");
  for (auto &s : shown) CHECK(s.alpha == 1.0f);

  shown = log.visible(11.5); // line 2 arrived at 2: 9.5 s old, half faded
  CHECK(shown.front().line->text == "2");
  CHECK(shown.front().alpha == doctest::Approx(0.5f));
  CHECK(log.visible(30.0).empty());
}
