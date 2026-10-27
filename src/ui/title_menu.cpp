#include "ui/title_menu.hpp"
#include "net/wire.hpp"
#include "sim/game.hpp"
#include "core/settings.hpp"
#include "imgui/imgui.h"
#include "ui/menu_style.hpp"
#include <cctype>
#include <charconv>
#include <cstdio>
#include <cstring>

std::optional<HostAddress> parseAddress(std::string_view text, uint16_t default_port) {
  while (!text.empty() && text.front() == ' ') text.remove_prefix(1);
  while (!text.empty() && text.back() == ' ') text.remove_suffix(1);
  if (text.empty()) return std::nullopt;
  HostAddress out{std::string(text), default_port};
  size_t colon = text.rfind(':');
  if (colon != std::string_view::npos) {
    std::string_view port = text.substr(colon + 1);
    unsigned value = 0;
    auto [end, ec] = std::from_chars(port.data(), port.data() + port.size(), value);
    if (ec != std::errc() || end != port.data() + port.size() || value == 0 || value > 65535)
      return std::nullopt;
    out.host = std::string(text.substr(0, colon));
    out.port = static_cast<uint16_t>(value);
  }
  if (out.host.empty() || out.host.find(' ') != std::string::npos) return std::nullopt;
  return out;
}

std::string sizeText(uintmax_t bytes) {
  char buf[32];
  if (bytes < 1024) std::snprintf(buf, sizeof buf, "%ju B", bytes);
  else if (bytes < 1024 * 1024) std::snprintf(buf, sizeof buf, "%ju KB", bytes / 1024);
  else std::snprintf(buf, sizeof buf, "%.1f MB", static_cast<double>(bytes) / (1024.0 * 1024.0));
  return buf;
}

const char *modeLabel(GameMode mode) {
  return mode == GameMode::Creative ? "Creative" : "Survival";
}

std::string gameLine(const discovery::Beacon &b) {
  std::string mode = modeLabel(b.mode);
  for (char &c : mode) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return b.world + "  -  " + b.host + "'s game, " + mode + "  (" + std::to_string(b.players) +
         "/" + std::to_string(b.max_players) + ")";
}

namespace {

const ImVec4 kWarn(1.f, 0.6f, 0.3f, 1.f);
const ImVec4 kError(1.f, 0.45f, 0.4f, 1.f);

#ifndef VOXEL_VERSION
#define VOXEL_VERSION "dev"
#endif

// Esc, unless a text box has it (Esc there just leaves the box).
bool escPressed() {
  return ImGui::IsKeyPressed(ImGuiKey_Escape, false) && !ImGui::IsAnyItemActive();
}

} // namespace

void TitleMenu::open(std::string message) {
  message_ = std::move(message);
  status_.clear();
  synced_name = false;
  form = Form::None;
  selected = -1; // re-read the world list: a game may have just been saved
  std::string error;
  browsing = browser.start(error);
}

void TitleMenu::close() {
  browser = discovery::LanBrowser{}; // stop listening
  browsing = false;
}

void TitleMenu::refreshWorlds(const std::string &select) {
  worlds = listWorlds(root);
  selected = worlds.empty() ? -1 : 0;
  for (int i = 0; i < static_cast<int>(worlds.size()); ++i)
    if (worlds[i].name == select) selected = i;
}

TitleMenu::Choice TitleMenu::draw(double now, std::string &name, std::string &last_world,
                                  uint16_t default_port) {
  if (!synced_name) {
    std::snprintf(name_buf, sizeof name_buf, "%s", name.c_str());
    synced_name = true;
  }
  if (browsing) browser.poll(now);

  drawTitle();
  drawVersion();
  Choice choice;
  switch (page) {
    case Page::Main: drawMain(choice, name); break;
    case Page::Worlds: drawWorlds(choice, last_world); break;
    case Page::Multiplayer: drawMultiplayer(choice, default_port); break;
    case Page::Settings:
      if (settings.draw(g_settings, false)) page = Page::Main;
      break;
  }
  if (page != Page::Main && escPressed()) goBack();
  if (choice.kind == Choice::Play || choice.kind == Choice::Host) last_world = choice.world;
  return choice;
}

void TitleMenu::drawTitle() {
  const ImGuiIO &io = ImGui::GetIO();
  ImDrawList *draw = ImGui::GetForegroundDrawList();
  const char *title = "GRUMBLEGROVE";
  float size = std::max(48.0f, io.DisplaySize.y * 0.11f);
  ImVec2 extent = ImGui::GetFont()->CalcTextSizeA(size, FLT_MAX, 0.0f, title);
  ImVec2 at(io.DisplaySize.x * 0.5f - extent.x * 0.5f, io.DisplaySize.y * 0.12f);
  draw->AddText(ImGui::GetFont(), size, ImVec2(at.x + 4, at.y + 4), IM_COL32(0, 0, 0, 150), title);
  draw->AddText(ImGui::GetFont(), size, at, IM_COL32(250, 245, 225, 255), title);

  const char *subtitle = "build  -  explore  -  survive";
  float sub_size = std::max(16.0f, size * 0.22f);
  ImVec2 sub_ext = ImGui::GetFont()->CalcTextSizeA(sub_size, FLT_MAX, 0.0f, subtitle);
  ImVec2 sub_at(io.DisplaySize.x * 0.5f - sub_ext.x * 0.5f, at.y + extent.y + sub_size * 0.2f);
  draw->AddText(ImGui::GetFont(), sub_size, ImVec2(sub_at.x + 2, sub_at.y + 2),
                IM_COL32(0, 0, 0, 140), subtitle);
  draw->AddText(ImGui::GetFont(), sub_size, sub_at, IM_COL32(240, 190, 120, 255), subtitle);
}

void TitleMenu::drawVersion() {
  const ImGuiIO &io = ImGui::GetIO();
  const char *text = "Grumblegrove " VOXEL_VERSION;
  float size = 15.0f;
  ImVec2 ext = ImGui::GetFont()->CalcTextSizeA(size, FLT_MAX, 0.0f, text);
  ImGui::GetForegroundDrawList()->AddText(
      ImGui::GetFont(), size, ImVec2(io.DisplaySize.x - ext.x - 12, io.DisplaySize.y - ext.y - 8),
      IM_COL32(255, 255, 255, 120), text);
}

// Esc and Back: close an open form first, then return to the main page.
void TitleMenu::goBack() {
  if (page == Page::Worlds && form != Form::None) {
    form = Form::None;
    return;
  }
  if (page == Page::Settings) settings.close();
  if (page == Page::Worlds) selected = -1;
  page = Page::Main;
  form = Form::None;
}

void TitleMenu::drawMain(Choice &choice, std::string &name) {
  beginMenuPanel("##main", 400);
  ImGui::InputText("Your name", name_buf, sizeof name_buf);
  name = name_buf;
  bool name_ok = Game::validName(name);
  if (!name_ok) ImGui::TextColored(kWarn, "1-16 letters, digits, _ or -");

  ImGui::Spacing();
  ImGui::BeginDisabled(!name_ok);
  if (menuButton("Single player")) {
    page = Page::Worlds;
    form = Form::None;
    message_.clear();
  }
  if (ImGui::IsWindowAppearing()) ImGui::SetItemDefaultFocus();
  if (menuButton("Multiplayer")) {
    page = Page::Multiplayer;
    message_.clear();
  }
  ImGui::EndDisabled();
  if (menuButton("Settings")) {
    page = Page::Settings;
    message_.clear();
  }
  ImGui::Spacing();
  if (menuButton("Quit")) choice.kind = Choice::Quit;

  if (!status_.empty()) ImGui::TextUnformatted(status_.c_str());
  if (!message_.empty()) ImGui::TextColored(kError, "%s", message_.c_str());
  endMenuPanel();
}

void TitleMenu::drawWorlds(Choice &choice, std::string &last_world) {
  // Refresh on arrival (and keep the last played one selected).
  if (selected < 0 || selected >= static_cast<int>(worlds.size())) refreshWorlds(last_world);

  beginMenuPanel("##worlds", 780);
  ImGui::TextUnformatted("Your worlds");
  ImGui::Separator();

  auto now = std::chrono::system_clock::now();
  if (ImGui::BeginTable("##list", 5,
                        ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY |
                            ImGuiTableFlags_BordersInnerV,
                        ImVec2(0, 220))) {
    ImGui::TableSetupColumn("World", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn("Mode", ImGuiTableColumnFlags_WidthFixed, 90);
    ImGui::TableSetupColumn("Seed", ImGuiTableColumnFlags_WidthFixed, 110);
    ImGui::TableSetupColumn("Last played", ImGuiTableColumnFlags_WidthFixed, 150);
    ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed, 80);
    ImGui::TableHeadersRow();
    for (int i = 0; i < static_cast<int>(worlds.size()); ++i) {
      const WorldInfo &w = worlds[i];
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::PushID(i);
      if (ImGui::Selectable(w.name.c_str(), i == selected,
                            ImGuiSelectableFlags_SpanAllColumns |
                                ImGuiSelectableFlags_AllowDoubleClick)) {
        selected = i;
        form = Form::None;
        if (ImGui::IsMouseDoubleClicked(0)) choice = {Choice::Play, w.name, {}};
      }
      ImGui::PopID();
      ImGui::TableNextColumn();
      ImGui::TextUnformatted(modeLabel(w.mode));
      ImGui::TableNextColumn();
      ImGui::Text("%u", w.seed);
      ImGui::TableNextColumn();
      ImGui::TextUnformatted(lastPlayedText(toSystemTime(w.last_played), now).c_str());
      ImGui::TableNextColumn();
      ImGui::TextUnformatted(sizeText(w.size_bytes).c_str());
    }
    ImGui::EndTable();
  }
  if (worlds.empty()) ImGui::TextDisabled("No worlds yet: create one below.");

  bool has = selected >= 0 && selected < static_cast<int>(worlds.size());
  std::string sel = has ? worlds[selected].name : std::string();
  float spacing = ImGui::GetStyle().ItemSpacing.x;
  float half = (ImGui::GetContentRegionAvail().x - spacing) / 2;
  float quarter = (ImGui::GetContentRegionAvail().x - 3 * spacing) / 4;

  ImGui::Spacing();
  ImGui::BeginDisabled(!has);
  if (ImGui::Button("Play", ImVec2(half, 38))) choice = {Choice::Play, sel, {}};
  if (ImGui::IsWindowAppearing()) ImGui::SetItemDefaultFocus();
  ImGui::SameLine();
  if (ImGui::Button("Host on network", ImVec2(-1, 38))) choice = {Choice::Host, sel, {}};
  ImGui::EndDisabled();

  if (ImGui::Button("Create new world", ImVec2(quarter, 0))) {
    form = Form::Create;
    world_buf[0] = seed_buf[0] = '\0';
    create_mode = GameMode::Survival;
    message_.clear();
  }
  ImGui::SameLine();
  ImGui::BeginDisabled(!has);
  if (ImGui::Button("Rename", ImVec2(quarter, 0))) {
    form = Form::Rename;
    std::snprintf(world_buf, sizeof world_buf, "%s", sel.c_str());
    message_.clear();
  }
  ImGui::SameLine();
  GameMode sel_mode = has ? worlds[selected].mode : GameMode::Survival;
  GameMode other = sel_mode == GameMode::Creative ? GameMode::Survival : GameMode::Creative;
  std::string change = std::string("Make ") + modeLabel(other);
  if (ImGui::Button(change.c_str(), ImVec2(quarter, 0))) {
    form = Form::None;
    if (auto err = setWorldMode(root, sel, other)) message_ = *err;
    else message_.clear();
    refreshWorlds(sel);
  }
  ImGui::SameLine();
  if (ImGui::Button("Delete", ImVec2(-1, 0))) {
    form = Form::Delete;
    message_.clear();
  }
  ImGui::EndDisabled();

  // The open form, under the buttons.
  if (form != Form::None) ImGui::Separator();
  if (form == Form::Create) {
    ImGui::InputText("World name", world_buf, sizeof world_buf);
    ImGui::InputTextWithHint("Seed", "blank = random", seed_buf, sizeof seed_buf);
    if (ImGui::RadioButton("Survival", create_mode == GameMode::Survival))
      create_mode = GameMode::Survival;
    ImGui::SameLine();
    if (ImGui::RadioButton("Creative", create_mode == GameMode::Creative))
      create_mode = GameMode::Creative;
    ImGui::SameLine();
    ImGui::TextDisabled(create_mode == GameMode::Creative ? "fly, unlimited blocks, no harm"
                                                          : "collect, build, stay alive");
    bool ok = validWorldName(world_buf);
    if (!ok && world_buf[0]) ImGui::TextColored(kWarn, "1-32 letters, digits, spaces, _ or -");
    ImGui::BeginDisabled(!ok);
    if (ImGui::Button("Create")) {
      if (auto err = createWorld(root, world_buf, seed_buf, create_mode)) {
        message_ = *err;
      } else {
        refreshWorlds(world_buf);
        form = Form::None;
      }
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) form = Form::None;
  } else if (form == Form::Rename && has) {
    ImGui::InputText("New name", world_buf, sizeof world_buf);
    bool ok = validWorldName(world_buf);
    if (!ok) ImGui::TextColored(kWarn, "1-32 letters, digits, spaces, _ or -");
    ImGui::BeginDisabled(!ok);
    if (ImGui::Button("Rename world")) {
      if (auto err = renameWorld(root, sel, world_buf)) {
        message_ = *err;
      } else {
        if (last_world == sel) last_world = world_buf;
        refreshWorlds(world_buf);
        form = Form::None;
      }
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) form = Form::None;
  } else if (form == Form::Delete && has) {
    ImGui::TextColored(kError, "Delete '%s'? This can't be undone.", sel.c_str());
    if (ImGui::Button("Yes, delete it")) {
      if (auto err = deleteWorld(root, sel)) message_ = *err;
      refreshWorlds(last_world);
      form = Form::None;
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) form = Form::None;
  }

  ImGui::Spacing();
  ImGui::Separator();
  if (ImGui::Button("Back", ImVec2(-1, 0))) {
    form = Form::None;
    goBack();
  }
  if (!status_.empty()) ImGui::TextUnformatted(status_.c_str());
  if (!message_.empty()) ImGui::TextColored(kError, "%s", message_.c_str());
  endMenuPanel();
}

void TitleMenu::drawMultiplayer(Choice &choice, uint16_t default_port) {
  beginMenuPanel("##multiplayer", 640);
  ImGui::TextUnformatted("Join a game");
  ImGui::Separator();
  if (!browsing) {
    ImGui::TextDisabled("Can't search the network; type the address below.");
  } else if (browser.games().empty()) {
    ImGui::TextDisabled("Looking for games on your network...");
  }
  for (const discovery::FoundGame &g : browser.games()) {
    const discovery::Beacon &b = g.beacon;
    bool same = b.version == wire::kProtocolVersion;
    bool full = b.players >= b.max_players;
    ImGui::PushID((g.address + ":" + std::to_string(b.port)).c_str());
    ImGui::BeginDisabled(!same || full);
    if (ImGui::Button("Join")) choice = {Choice::Join, {}, {g.address, b.port}};
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::Text("%s%s", gameLine(b).c_str(),
                !same ? "  different version" : full ? "  full" : "");
    ImGui::PopID();
  }

  ImGui::Spacing();
  ImGui::PushItemWidth(340);
  bool enter = ImGui::InputTextWithHint("##address", "address, e.g. 192.168.1.20", address_buf,
                                        sizeof address_buf, ImGuiInputTextFlags_EnterReturnsTrue);
  ImGui::PopItemWidth();
  ImGui::SameLine();
  auto typed = parseAddress(address_buf, default_port);
  ImGui::BeginDisabled(!typed);
  if ((ImGui::Button("Join address") || enter) && typed) choice = {Choice::Join, {}, *typed};
  ImGui::EndDisabled();

  ImGui::Spacing();
  ImGui::Separator();
  if (ImGui::Button("Back", ImVec2(-1, 0))) goBack();
  if (ImGui::IsWindowAppearing()) ImGui::SetItemDefaultFocus();
  if (!status_.empty()) ImGui::TextUnformatted(status_.c_str());
  if (!message_.empty()) ImGui::TextColored(kError, "%s", message_.c_str());
  endMenuPanel();
}
