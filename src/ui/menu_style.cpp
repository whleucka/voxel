#include "ui/menu_style.hpp"
#include "imgui/imgui.h"

void applyMenuStyle() {
  ImGuiStyle &s = ImGui::GetStyle();
  s.WindowRounding = 10.0f;
  s.FrameRounding = 6.0f;
  s.GrabRounding = 6.0f;
  s.PopupRounding = 6.0f;
  s.ScrollbarRounding = 6.0f;
  s.TabRounding = 6.0f;
  s.WindowPadding = ImVec2(18, 16);
  s.FramePadding = ImVec2(10, 6);
  s.ItemSpacing = ImVec2(10, 8);
  s.WindowBorderSize = 0.0f;

  ImVec4 *c = s.Colors;
  c[ImGuiCol_WindowBg] = ImVec4(0.09f, 0.10f, 0.13f, 1.00f);
  c[ImGuiCol_Button] = ImVec4(0.22f, 0.26f, 0.32f, 1.00f);
  c[ImGuiCol_ButtonHovered] = ImVec4(0.80f, 0.52f, 0.20f, 1.00f);
  c[ImGuiCol_ButtonActive] = ImVec4(0.93f, 0.63f, 0.26f, 1.00f);
  c[ImGuiCol_FrameBg] = ImVec4(0.16f, 0.18f, 0.23f, 1.00f);
  c[ImGuiCol_FrameBgHovered] = ImVec4(0.26f, 0.28f, 0.34f, 1.00f);
  c[ImGuiCol_FrameBgActive] = ImVec4(0.32f, 0.33f, 0.40f, 1.00f);
  c[ImGuiCol_SliderGrab] = ImVec4(0.85f, 0.58f, 0.24f, 1.00f);
  c[ImGuiCol_SliderGrabActive] = ImVec4(0.95f, 0.68f, 0.30f, 1.00f);
  c[ImGuiCol_CheckMark] = ImVec4(0.95f, 0.70f, 0.32f, 1.00f);
  c[ImGuiCol_Header] = ImVec4(0.80f, 0.52f, 0.20f, 0.45f);
  c[ImGuiCol_HeaderHovered] = ImVec4(0.80f, 0.52f, 0.20f, 0.70f);
  c[ImGuiCol_HeaderActive] = ImVec4(0.93f, 0.63f, 0.26f, 0.85f);
  c[ImGuiCol_NavCursor] = ImVec4(1.00f, 0.80f, 0.45f, 1.00f);
  c[ImGuiCol_Separator] = ImVec4(1.00f, 1.00f, 1.00f, 0.12f);
}

void beginMenuPanel(const char *id, float width, float y_frac) {
  const ImGuiIO &io = ImGui::GetIO();
  ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * y_frac),
                          ImGuiCond_Always, ImVec2(0.5f, 0.5f));
  ImGui::SetNextWindowSize(ImVec2(width, 0), ImGuiCond_Always);
  ImGui::SetNextWindowBgAlpha(0.82f);
  ImGui::Begin(id, nullptr,
               ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                   ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings |
                   ImGuiWindowFlags_NoTitleBar);
  ImGui::PushFont(nullptr, kMenuFontSize);
}

void endMenuPanel() {
  ImGui::PopFont();
  ImGui::End();
}

bool menuButton(const char *label, float height) {
  return ImGui::Button(label, ImVec2(-1, height));
}
