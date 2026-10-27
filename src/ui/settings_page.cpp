#include "ui/settings_page.hpp"
#include "core/config.hpp"
#include "core/constants.hpp"
#include "imgui/imgui.h"
#include "ui/menu_style.hpp"
#include <algorithm>

bool SettingsPage::draw(Settings &s, bool difficulty_locked) {
  beginMenuPanel("##settings", 480, 0.55f);
  ImGui::TextUnformatted("Settings");
  ImGui::Separator();
  ImGui::PushItemWidth(-170);

  // Game
  static const char *kDifficulties[] = {"Peaceful", "Easy", "Normal"};
  int difficulty = static_cast<int>(s.difficulty);
  ImGui::BeginDisabled(difficulty_locked);
  if (ImGui::Combo("Difficulty", &difficulty, kDifficulties, 3))
    s.difficulty = static_cast<Difficulty>(difficulty);
  ImGui::EndDisabled();
  if (difficulty_locked) ImGui::TextDisabled("The host chooses the difficulty.");

  // View
  int chunks = std::clamp(s.render_distance / kChunkWidth, kMinChunks, kMaxChunks);
  if (ImGui::SliderInt("Render distance", &chunks, kMinChunks, kMaxChunks, "%d chunks"))
    s.render_distance = chunks * kChunkWidth;
  ImGui::TextDisabled("Render distance applies the next time a world loads.");
  ImGui::SliderFloat("Field of view", &s.fov, 50.0f, 110.0f, "%.0f deg");
  ImGui::SliderFloat("Mouse sensitivity", &s.mouse_sensitivity, 0.01f, 0.30f, "%.3f");

  // Display
  ImGui::Checkbox("Fullscreen", &s.fullscreen);
  ImGui::SameLine(0, 40);
  ImGui::Checkbox("VSync", &s.vsync);
  ImGui::Checkbox("Clouds", &s.clouds_enabled);
  ImGui::SameLine(0, 40 + ImGui::CalcTextSize("Fullscreen").x - ImGui::CalcTextSize("Clouds").x);
  ImGui::Checkbox("Shadows", &s.shadows_enabled);
  ImGui::PopItemWidth();

  ImGui::Spacing();
  ImGui::Separator();
  // Also resets what only the F12 panel shows (time speed, fog, cloud and
  // shadow details), so a stray change there is easy to undo.
  if (!confirm_reset) {
    if (ImGui::Button("Reset to defaults", ImVec2(-1, 0))) confirm_reset = true;
  } else {
    ImGui::TextColored(ImVec4(1.f, 0.6f, 0.3f, 1.f),
                       "Reset every setting? Your name and worlds are kept.");
    if (ImGui::Button("Yes, reset")) {
      Difficulty kept = s.difficulty;
      s = defaultsKeepingIdentity(s);
      if (difficulty_locked) s.difficulty = kept; // the host's, not ours
      confirm_reset = false;
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) confirm_reset = false;
  }
  bool done = menuButton("Done");
  if (ImGui::IsWindowAppearing()) ImGui::SetItemDefaultFocus();
  endMenuPanel();
  if (done) close();
  return done;
}

void SettingsPage::close() {
  confirm_reset = false;
  saveServerProperties();
}
