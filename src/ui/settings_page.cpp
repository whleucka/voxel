#include "ui/settings_page.hpp"
#include "core/config.hpp"
#include "core/constants.hpp"
#include "imgui/imgui.h"
#include "ui/menu_style.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <string>

namespace {

SettingsPage::Preview g_preview;

std::string capitalized(const char *name) {
  std::string out = name;
  if (!out.empty()) out[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(out[0])));
  return out;
}

// One row of colours. The framebuffer encodes sRGB itself, so ImGui's colours
// go in linear, to match the figure.
void swatchRow(const char *label, LookPart part, PlayerLook &look, const std::string &name) {
  constexpr float kSize = 24.0f;
  ImGui::TextUnformatted(label);
  ImGui::PushID(label);
  uint8_t &value = lookField(look, part);
  std::span<const LookSwatch> swatches = lookSwatches(part);
  for (size_t i = 0; i < swatches.size(); ++i) {
    const bool is_auto = part == LookPart::Shirt && i == 0;
    const glm::vec3 c = is_auto ? playerColor(name) : swatches[i].color;
    if (i > 0) ImGui::SameLine(0, 4);
    ImGui::PushID(static_cast<int>(i));
    if (ImGui::ColorButton("##swatch",
                           ImVec4(std::pow(c.r, 2.2f), std::pow(c.g, 2.2f), std::pow(c.b, 2.2f), 1.0f),
                           ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoBorder,
                           ImVec2(kSize, kSize)))
      value = static_cast<uint8_t>(i);
    if (ImGui::IsItemHovered())
      ImGui::SetTooltip("%s", is_auto ? "Auto (from your name)" : capitalized(swatches[i].name).c_str());
    if (value == i)
      ImGui::GetWindowDrawList()->AddRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(),
                                          IM_COL32(255, 255, 255, 255), 3.0f, 0, 2.5f);
    ImGui::PopID();
  }
  ImGui::PopID();
}

} // namespace

void SettingsPage::setPreview(Preview preview) { g_preview = std::move(preview); }

bool SettingsPage::back() {
  if (!on_character) return false;
  on_character = false;
  saveServerProperties();
  return true;
}

void SettingsPage::drawCharacter(Settings &s) {
  constexpr float kFigureWidth = 170.0f, kFigureHeight = 270.0f;
  beginMenuPanel("##character", 540, 0.55f);
  ImGui::TextUnformatted("Character");
  ImGui::Separator();

  if (g_preview) {
    // Twice the pixels it's shown at, so the edges stay clean. Flipped: GL's
    // rows start at the bottom.
    unsigned int texture = g_preview(s.player_look, s.player_name, static_cast<int>(kFigureWidth * 2),
                                     static_cast<int>(kFigureHeight * 2));
    ImGui::Image(static_cast<ImTextureID>(texture), ImVec2(kFigureWidth, kFigureHeight),
                 ImVec2(0, 1), ImVec2(1, 0));
    ImGui::SameLine(0, 16);
  }
  ImGui::BeginGroup();
  for (uint8_t i = 0; i < std::size(kFigureNames); ++i) {
    if (i > 0) ImGui::SameLine(0, 16);
    if (ImGui::RadioButton(capitalized(kFigureNames[i]).c_str(), s.player_look.figure == i))
      s.player_look.figure = i;
  }
  ImGui::Spacing();
  swatchRow("Shirt", LookPart::Shirt, s.player_look, s.player_name);
  swatchRow("Hair", LookPart::Hair, s.player_look, s.player_name);
  swatchRow("Skin", LookPart::Skin, s.player_look, s.player_name);
  swatchRow("Eyes", LookPart::Eyes, s.player_look, s.player_name);
  ImGui::EndGroup();

  ImGui::Spacing();
  bool done = menuButton("Done");
  endMenuPanel();
  if (done) back();
}

bool SettingsPage::draw(Settings &s, bool difficulty_locked) {
  if (on_character) {
    drawCharacter(s);
    return false;
  }
  beginMenuPanel("##settings", 480, 0.55f);
  ImGui::TextUnformatted("Settings");
  ImGui::Separator();
  ImGui::PushItemWidth(-170);

  // Game
  static const char *kDifficulties[] = {"Peaceful", "Easy", "Normal", "Hard"};
  int difficulty = static_cast<int>(s.difficulty);
  ImGui::BeginDisabled(difficulty_locked);
  if (ImGui::Combo("Difficulty", &difficulty, kDifficulties, 4))
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
  ImGui::Checkbox("Minimap", &s.show_minimap);
  ImGui::PopItemWidth();

  ImGui::Spacing();
  if (ImGui::Button("Character", ImVec2(-1, 0))) on_character = true;
  ImGui::Separator();
  // Also resets what only the F12 panel shows (time speed, fog, cloud and
  // shadow details), so a stray change there is easy to undo.
  if (!confirm_reset) {
    if (ImGui::Button("Reset to defaults", ImVec2(-1, 0))) confirm_reset = true;
  } else {
    ImGui::TextColored(ImVec4(1.f, 0.6f, 0.3f, 1.f),
                       "Reset every setting? Your name, character and worlds are kept.");
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
  on_character = false;
  saveServerProperties();
}
