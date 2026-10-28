#pragma once

#include "sim/chat.hpp"
#include "ui/hud_layout.hpp"
#include <cstddef>
#include <optional>
#include <string>

// Chat on screen (ImGui). The log and the rules are in sim/chat.hpp.

// Room for kMaxChatChars characters of up to 4 bytes, and the terminator.
constexpr size_t kChatBufBytes = kMaxChatChars * 4 + 1;

// Recent lines over the game while chat is closed, fading out.
void drawChatLines(const Rect &area, const ChatLog &log, double now);

struct ChatPanelResult {
  std::optional<std::string> sent; // Enter or Send with something typed
  bool close = false;              // sent, or Enter on an empty box
};

// The open panel: history, a text box and a Send button. `opening` is true
// on the frame it opens, to focus the box (and drop the T that opened it).
ChatPanelResult drawChatPanel(const Rect &panel, const ChatLog &log, char *buf,
                              bool opening);
