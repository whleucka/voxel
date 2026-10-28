#include "ui/chat_view.hpp"
#include "imgui/imgui.h"
#include <cstring>

namespace {

constexpr ImU32 kNewsColor = IM_COL32(190, 190, 190, 255);
constexpr ImVec4 kTimeColor{0.65f, 0.65f, 0.65f, 1.0f};

// One line: "[7:42 pm] Aria: text", wrapping onto the next line.
void chatLine(const ChatLine &line) {
  ImGui::TextColored(kTimeColor, "[%s]", formatClock(line.minute).c_str());
  ImGui::SameLine();
  if (line.from == 0) {
    ImGui::PushStyleColor(ImGuiCol_Text, kNewsColor);
    ImGui::TextWrapped("%s", line.text.c_str());
    ImGui::PopStyleColor();
    return;
  }
  glm::vec3 c = chatNameColor(line.from);
  ImGui::TextColored(ImVec4(c.r, c.g, c.b, 1.0f), "%s:", line.name.c_str());
  ImGui::SameLine();
  ImGui::TextWrapped("%s", line.text.c_str());
}

size_t utf8Chars(const char *s) {
  size_t n = 0;
  for (; *s; ++s) n += (static_cast<unsigned char>(*s) & 0xC0) != 0x80;
  return n;
}

// Stops typing at kMaxChatChars (pasting goes through here too).
int limitChars(ImGuiInputTextCallbackData *data) {
  const char *buf = static_cast<const char *>(data->UserData);
  return utf8Chars(buf) >= kMaxChatChars ? 1 : 0;
}

constexpr ImGuiWindowFlags kFixed = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                                    ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                                    ImGuiWindowFlags_NoSavedSettings;

} // namespace

void drawChatLines(const Rect &area, const ChatLog &log, double now) {
  auto shown = log.visible(now);
  if (shown.empty()) return;

  ImGui::SetNextWindowPos(ImVec2(area.x, area.bottom()), ImGuiCond_Always, ImVec2(0.0f, 1.0f));
  ImGui::SetNextWindowSizeConstraints(ImVec2(area.w, 0), ImVec2(area.w, area.h));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6, 4));
  ImGui::Begin("##chatlines", nullptr,
               kFixed | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoBackground |
                   ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing |
                   ImGuiWindowFlags_NoNav);
  ImGui::PushTextWrapPos(area.w - 6);
  ImDrawList *dl = ImGui::GetWindowDrawList();
  for (const ChatLog::Shown &s : shown) {
    // A dark strip behind each line, fading with it: text on channel 1,
    // its backing on channel 0.
    dl->ChannelsSplit(2);
    dl->ChannelsSetCurrent(1);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, s.alpha);
    ImGui::BeginGroup();
    chatLine(*s.line);
    ImGui::EndGroup();
    ImGui::PopStyleVar();
    dl->ChannelsSetCurrent(0);
    ImVec2 lo = ImGui::GetItemRectMin(), hi = ImGui::GetItemRectMax();
    dl->AddRectFilled(ImVec2(area.x, lo.y - 1), ImVec2(area.right(), hi.y + 1),
                      IM_COL32(0, 0, 0, static_cast<int>(110 * s.alpha)));
    dl->ChannelsMerge();
  }
  ImGui::PopTextWrapPos();
  ImGui::End();
  ImGui::PopStyleVar();
}

ChatPanelResult drawChatPanel(const Rect &panel, const ChatLog &log, char *buf, bool opening) {
  ChatPanelResult result;
  ImGui::SetNextWindowPos(ImVec2(panel.x, panel.y), ImGuiCond_Always);
  ImGui::SetNextWindowSize(ImVec2(panel.w, panel.h), ImGuiCond_Always);
  ImGui::SetNextWindowBgAlpha(0.6f);
  ImGui::Begin("##chat", nullptr, kFixed);

  float row = ImGui::GetFrameHeightWithSpacing();
  ImGui::BeginChild("##history", ImVec2(0, -row), ImGuiChildFlags_None);
  for (const ChatLine &line : log.lines()) chatLine(line);
  // Follow new lines, unless scrolled up to read older ones.
  if (opening || ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 1.0f)
    ImGui::SetScrollHereY(1.0f);
  ImGui::EndChild();

  if (opening) {
    ImGui::GetIO().ClearInputKeys(); // the T that opened chat isn't typed
    ImGui::SetKeyboardFocusHere();
  }
  const char *send = "Send";
  float send_w = ImGui::CalcTextSize(send).x + ImGui::GetStyle().FramePadding.x * 2;
  ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - send_w -
                          ImGui::GetStyle().ItemSpacing.x);
  bool enter = ImGui::InputTextWithHint("##say", "Say something...", buf, kChatBufBytes,
                                        ImGuiInputTextFlags_EnterReturnsTrue |
                                            ImGuiInputTextFlags_CallbackCharFilter,
                                        limitChars, buf);
  ImGui::SameLine();
  bool clicked = ImGui::Button(send);
  if (enter || clicked) {
    if (std::optional<std::string> text = cleanChatText(buf)) result.sent = *text;
    // Enter always closes; Send with nothing typed keeps the box open.
    result.close = enter || result.sent.has_value();
    buf[0] = '\0';
  }
  ImGui::End();
  return result;
}
