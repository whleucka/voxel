#include "ui/pause_menu.hpp"
#include "core/settings.hpp"
#include "imgui/imgui.h"
#include "ui/menu_style.hpp"

void PauseMenu::escape() {
  switch (page_) {
    case Page::Closed: page_ = Page::Main; break;
    case Page::Main: page_ = Page::Closed; break;
    case Page::Settings:
      settings.close();
      page_ = Page::Main;
      break;
  }
}

PauseMenu::Action PauseMenu::press(Button b) {
  switch (b) {
    case Button::Resume: page_ = Page::Closed; return Action::None;
    case Button::Settings: page_ = Page::Settings; return Action::None;
    case Button::Save: return Action::Save;
    case Button::ReturnToTitle: page_ = Page::Closed; return Action::ReturnToTitle;
    case Button::Exit: page_ = Page::Closed; return Action::Exit;
  }
  return Action::None;
}

PauseMenu::Action PauseMenu::draw(bool single_player, bool guest, double now) {
  if (!isOpen()) return Action::None;
  ImGui::GetBackgroundDrawList()->AddRectFilled(ImVec2(0, 0), ImGui::GetIO().DisplaySize,
                                                IM_COL32(0, 0, 0, 110));
  if (page_ == Page::Settings) {
    if (settings.draw(g_settings, guest)) page_ = Page::Main;
    return Action::None;
  }

  Action action = Action::None;
  beginMenuPanel("##pause", 340, 0.5f);
  const char *title = single_player ? "Paused" : "Menu";
  ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ImGui::CalcTextSize(title).x) * 0.5f);
  ImGui::TextUnformatted(title);
  ImGui::Separator();
  if (menuButton("Resume")) action = press(Button::Resume);
  if (ImGui::IsWindowAppearing()) ImGui::SetItemDefaultFocus();
  if (menuButton("Settings")) action = press(Button::Settings);
  ImGui::BeginDisabled(!canSave(guest));
  if (menuButton(now - saved_at < 2.0 ? "Saved" : "Save game")) action = press(Button::Save);
  ImGui::EndDisabled();
  ImGui::Spacing();
  if (menuButton("Return to title screen")) action = press(Button::ReturnToTitle);
  if (menuButton("Exit game")) action = press(Button::Exit);
  if (guest) ImGui::TextDisabled("The host saves this world.");
  if (!single_player) ImGui::TextDisabled("The game keeps running for other players.");
  endMenuPanel();
  return action;
}
