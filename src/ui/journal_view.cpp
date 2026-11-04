#include "ui/journal_view.hpp"
#include "imgui/imgui.h"
#include "ui/hud.hpp"
#include "ui/menu_style.hpp"
#include <algorithm>
#include <cmath>
#include <string>

namespace {

constexpr float kCellW = 108.0f;
constexpr float kCellH = 104.0f;
constexpr float kIcon = 64.0f;

const ImVec4 kInk(1.00f, 0.86f, 0.60f, 1.0f);    // titles
const ImVec4 kChosen(0.93f, 0.63f, 0.26f, 0.85f); // the open page's button

} // namespace

bool JournalView::draw(const Hud &hud, const SeenItems &seen, const JournalProgress &journal,
                       bool creative) {
  const ImGuiIO &io = ImGui::GetIO();
  const ImVec2 size(std::min(780.0f, io.DisplaySize.x - 32.0f),
                    std::min(580.0f, io.DisplaySize.y - 32.0f));
  ImGui::GetBackgroundDrawList()->AddRectFilled(ImVec2(0, 0), io.DisplaySize,
                                                IM_COL32(0, 0, 0, 90));
  ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f),
                          ImGuiCond_Always, ImVec2(0.5f, 0.5f));
  ImGui::SetNextWindowSize(size, ImGuiCond_Always);
  ImGui::SetNextWindowBgAlpha(0.9f);
  ImGui::Begin("##journal", nullptr,
               ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                   ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings |
                   ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar);
  ImGui::PushFont(nullptr, kMenuFontSize);

  ImGui::PushStyleColor(ImGuiCol_Text, kInk);
  ImGui::PushFont(nullptr, kMenuFontSize * 1.4f);
  ImGui::TextUnformatted("My Journal");
  ImGui::PopFont();
  ImGui::PopStyleColor();
  ImGui::SameLine();
  const float close_w = 90.0f;
  ImGui::SetCursorPosX(ImGui::GetWindowWidth() - close_w - ImGui::GetStyle().WindowPadding.x);
  const bool close = ImGui::Button("Close", ImVec2(close_w, 0));
  if (creative) ImGui::TextDisabled("The journal only writes in survival worlds.");

  if (ImGui::BeginTabBar("##journal_tabs")) {
    // Open on the tab last used.
    auto flags = [&](Tab t) {
      return ImGui::IsWindowAppearing() && tab_ == t ? ImGuiTabItemFlags_SetSelected
                                                     : ImGuiTabItemFlags_None;
    };
    if (ImGui::BeginTabItem("Notes", nullptr, flags(Tab::Notes))) {
      tab_ = Tab::Notes;
      drawNotes(seen, journal);
      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("Collections", nullptr, flags(Tab::Collections))) {
      tab_ = Tab::Collections;
      drawCollections(hud, seen, journal);
      ImGui::EndTabItem();
    }
    ImGui::EndTabBar();
  }

  ImGui::PopFont();
  ImGui::End();
  return close;
}

void JournalView::drawNotes(const SeenItems &seen, const JournalProgress &journal) {
  ImGui::BeginChild("##notes", ImVec2(0, 0), ImGuiChildFlags_None);
  const ImVec4 done_col(0.62f, 0.90f, 0.55f, 1.0f);
  int locked = 0;
  for (int i = 0; i < static_cast<int>(chains().size()); ++i) {
    if (!chainOpen(i, seen, journal)) {
      ++locked;
      continue;
    }
    const Chain &chain = chains()[i];
    const bool stamped = journal.stamps.test(static_cast<size_t>(i));
    const int done = stepsDone(chain, seen, journal);
    const int steps = static_cast<int>(chain.steps.size());

    ImGui::PushID(i);
    ImGui::BeginGroup();
    const ImVec2 card0 = ImGui::GetCursorScreenPos();
    ImGui::Spacing();
    ImGui::Indent(10.0f);
    ImGui::PushStyleColor(ImGuiCol_Text, kInk);
    ImGui::Text("%.*s", static_cast<int>(chain.title.size()), chain.title.data());
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::TextDisabled("%d/%d", std::min(done, steps), steps);
    const float wrap = ImGui::GetContentRegionAvail().x - 110.0f;
    for (int s = 0; s < steps && s <= done; ++s) {
      const Step &step = chain.steps[s];
      const bool ticked = s < done;
      ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + wrap);
      if (ticked) {
        ImGui::PushStyleColor(ImGuiCol_Text, done_col);
        ImGui::Text("[x]  %.*s", static_cast<int>(step.done.size()), step.done.data());
        ImGui::PopStyleColor();
      } else {
        ImGui::Text("[ ]  %.*s", static_cast<int>(step.hint.size()), step.hint.data());
      }
      ImGui::PopTextWrapPos();
    }
    if (done + 1 < steps) ImGui::TextDisabled("     ...and more to find out.");
    if (!stamped)
      ImGui::TextDisabled("Finish it for %.*s.", static_cast<int>(chain.gift_text.size()),
                          chain.gift_text.data());
    ImGui::Unindent(10.0f);
    ImGui::Spacing();
    ImGui::EndGroup();
    ImGui::PopID();

    // The card behind it, and the stamp on a finished one.
    ImDrawList *dl = ImGui::GetWindowDrawList();
    const ImVec2 card1(card0.x + ImGui::GetContentRegionAvail().x, ImGui::GetItemRectMax().y);
    dl->AddRect(card0, card1, IM_COL32(255, 220, 160, 60), 6.0f);
    if (stamped) {
      const float r = 34.0f;
      const ImVec2 c(card1.x - r - 14.0f, card0.y + (card1.y - card0.y) * 0.5f);
      const ImU32 red = IM_COL32(220, 70, 60, 220);
      dl->AddCircle(c, r, red, 32, 3.0f);
      dl->AddCircle(c, r - 6.0f, red, 32, 1.5f);
      const char *word = "DONE!";
      const float size = ImGui::GetFontSize() * 0.95f;
      const ImVec2 ext = ImGui::GetFont()->CalcTextSizeA(size, FLT_MAX, 0.0f, word);
      dl->AddText(nullptr, size, ImVec2(c.x - ext.x * 0.5f, c.y - ext.y * 0.5f), red, word);
    }
    ImGui::Spacing();
  }
  if (locked) {
    ImGui::Spacing();
    ImGui::TextDisabled("More notes will turn up once I've made a Crafting Table.");
  }
  ImGui::EndChild();
}

void JournalView::drawCollections(const Hud &hud, const SeenItems &seen,
                                  const JournalProgress &journal) {
  ImGui::Spacing();
  for (int p = 0; p < kPageCount; ++p) {
    const JournalPage page = static_cast<JournalPage>(p);
    const PageCount count = pageCount(page, seen, journal);
    std::string label = std::string(pageTitle(page)) + "  " + std::to_string(count.filled) +
                        "/" + std::to_string(count.total);
    if (count.filled == count.total) label += " *";
    if (p) ImGui::SameLine();
    const bool chosen = page == page_;
    if (chosen) ImGui::PushStyleColor(ImGuiCol_Button, kChosen);
    if (ImGui::Button(label.c_str())) page_ = page;
    if (chosen) ImGui::PopStyleColor();
  }
  ImGui::Spacing();

  ImGui::BeginChild("##entries", ImVec2(0, 0), ImGuiChildFlags_None);
  const float avail = ImGui::GetContentRegionAvail().x;
  const int columns = std::max(1, static_cast<int>(avail / kCellW));
  ImDrawList *dl = ImGui::GetWindowDrawList();
  int i = 0;
  for (const Entry &e : entries()) {
    if (e.page != page_) continue;
    if (i % columns) ImGui::SameLine(0, 0);
    ++i;
    const bool found = entryFilled(e, seen, journal);
    ImGui::PushID(&e);
    const ImVec2 at = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton("##entry", ImVec2(kCellW, kCellH));
    const bool hovered = ImGui::IsItemHovered();
    ImGui::PopID();

    const ImVec2 box0(at.x + 4, at.y + 4), box1(at.x + kCellW - 4, at.y + kCellH - 4);
    dl->AddRectFilled(box0, box1,
                      hovered ? IM_COL32(255, 220, 160, 40) : IM_COL32(0, 0, 0, 70), 6.0f);
    const Rect icon{at.x + (kCellW - kIcon) * 0.5f, at.y + 6, kIcon, kIcon};
    const float tint = found ? 1.0f : 0.0f;
    const float alpha = found ? 1.0f : 0.55f;
    if (const auto *k = std::get_if<EntityKind>(&e.what))
      hud.drawPortrait(dl, icon, *k, alpha, tint);
    else
      hud.drawItemIcon(dl, icon, std::get<ItemId>(e.what), alpha, tint);

    const std::string name = found ? std::string(entryName(e)) : "???";
    const float font = ImGui::GetFontSize() * 0.8f;
    const ImVec2 ext = ImGui::GetFont()->CalcTextSizeA(font, FLT_MAX, 0.0f, name.c_str());
    const float x = at.x + std::max(4.0f, (kCellW - ext.x) * 0.5f);
    dl->PushClipRect(box0, box1, true);
    dl->AddText(nullptr, font, ImVec2(x, at.y + kIcon + 12),
                found ? IM_COL32(255, 255, 255, 255) : IM_COL32(170, 170, 170, 255),
                name.c_str());
    dl->PopClipRect();

    if (hovered) {
      ImGui::BeginTooltip();
      ImGui::TextUnformatted(name.c_str());
      ImGui::TextDisabled("%.*s", static_cast<int>(e.hint.size()), e.hint.data());
      ImGui::EndTooltip();
    }
  }
  ImGui::EndChild();
}
