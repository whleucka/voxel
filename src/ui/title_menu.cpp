#include "ui/title_menu.hpp"
#include "net/wire.hpp"
#include "sim/game.hpp"
#include "imgui/imgui.h"
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

namespace {

const ImVec4 kWarn(1.f, 0.6f, 0.3f, 1.f);
const ImVec4 kError(1.f, 0.45f, 0.4f, 1.f);
constexpr float kPanelAlpha = 0.78f;

// A panel centred on screen (a little below the title).
bool beginPanel(const char *id, float width) {
  const ImGuiIO &io = ImGui::GetIO();
  ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.58f),
                          ImGuiCond_Always, ImVec2(0.5f, 0.5f));
  ImGui::SetNextWindowSize(ImVec2(width, 0), ImGuiCond_Always);
  ImGui::SetNextWindowBgAlpha(kPanelAlpha);
  return ImGui::Begin(id, nullptr,
                      ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                          ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings |
                          ImGuiWindowFlags_NoTitleBar);
}

} // namespace

void TitleMenu::open(std::string message) {
  message_ = std::move(message);
  status_.clear();
  synced_name = false;
  form = Form::None;
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
  Choice choice;
  switch (page) {
    case Page::Main: drawMain(choice, name); break;
    case Page::Worlds: drawWorlds(choice, last_world); break;
    case Page::Multiplayer: drawMultiplayer(choice, default_port); break;
  }
  if (choice.kind == Choice::Play || choice.kind == Choice::Host) last_world = choice.world;
  return choice;
}

void TitleMenu::drawTitle() {
  const ImGuiIO &io = ImGui::GetIO();
  ImDrawList *draw = ImGui::GetForegroundDrawList();
  const char *title = "VOXEL";
  float size = std::max(48.0f, io.DisplaySize.y * 0.11f);
  ImVec2 extent = ImGui::GetFont()->CalcTextSizeA(size, FLT_MAX, 0.0f, title);
  ImVec2 at(io.DisplaySize.x * 0.5f - extent.x * 0.5f, io.DisplaySize.y * 0.12f);
  draw->AddText(ImGui::GetFont(), size, ImVec2(at.x + 4, at.y + 4), IM_COL32(0, 0, 0, 150), title);
  draw->AddText(ImGui::GetFont(), size, at, IM_COL32(250, 245, 225, 255), title);
}

void TitleMenu::drawMain(Choice &choice, std::string &name) {
  beginPanel("##main", 360);
  ImGui::InputText("Your name", name_buf, sizeof name_buf);
  name = name_buf;
  bool name_ok = Game::validName(name);
  if (!name_ok) ImGui::TextColored(kWarn, "1-16 letters, digits, _ or -");

  ImGui::Spacing();
  ImVec2 wide(-1, 36);
  ImGui::BeginDisabled(!name_ok);
  if (ImGui::Button("Single player", wide)) {
    page = Page::Worlds;
    form = Form::None;
    message_.clear();
  }
  if (ImGui::Button("Multiplayer", wide)) {
    page = Page::Multiplayer;
    message_.clear();
  }
  ImGui::EndDisabled();
  ImGui::Spacing();
  if (ImGui::Button("Quit", wide)) choice.kind = Choice::Quit;

  if (!status_.empty()) ImGui::TextUnformatted(status_.c_str());
  if (!message_.empty()) ImGui::TextColored(kError, "%s", message_.c_str());
  ImGui::End();
}

void TitleMenu::drawWorlds(Choice &choice, std::string &last_world) {
  // Refresh on arrival (and keep the last played one selected).
  if (selected < 0 || selected >= static_cast<int>(worlds.size())) refreshWorlds(last_world);

  beginPanel("##worlds", 620);
  ImGui::TextUnformatted("Your worlds");
  ImGui::Separator();

  auto now = std::chrono::system_clock::now();
  if (ImGui::BeginTable("##list", 4,
                        ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY |
                            ImGuiTableFlags_BordersInnerV,
                        ImVec2(0, 220))) {
    ImGui::TableSetupColumn("World", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn("Seed", ImGuiTableColumnFlags_WidthFixed, 100);
    ImGui::TableSetupColumn("Last played", ImGuiTableColumnFlags_WidthFixed, 130);
    ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed, 70);
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
  float third = (ImGui::GetContentRegionAvail().x - 2 * ImGui::GetStyle().ItemSpacing.x) / 3;

  ImGui::Spacing();
  ImGui::BeginDisabled(!has);
  if (ImGui::Button("Play", ImVec2(third * 1.5f, 34))) choice = {Choice::Play, sel, {}};
  ImGui::SameLine();
  if (ImGui::Button("Host on network", ImVec2(-1, 34))) choice = {Choice::Host, sel, {}};
  ImGui::EndDisabled();

  if (ImGui::Button("Create new world", ImVec2(third, 0))) {
    form = Form::Create;
    world_buf[0] = seed_buf[0] = '\0';
    message_.clear();
  }
  ImGui::SameLine();
  ImGui::BeginDisabled(!has);
  if (ImGui::Button("Rename", ImVec2(third, 0))) {
    form = Form::Rename;
    std::snprintf(world_buf, sizeof world_buf, "%s", sel.c_str());
    message_.clear();
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
    bool ok = validWorldName(world_buf);
    if (!ok && world_buf[0]) ImGui::TextColored(kWarn, "1-32 letters, digits, spaces, _ or -");
    ImGui::BeginDisabled(!ok);
    if (ImGui::Button("Create")) {
      if (auto err = createWorld(root, world_buf, seed_buf)) {
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
    page = Page::Main;
    form = Form::None;
    selected = -1;
  }
  if (!status_.empty()) ImGui::TextUnformatted(status_.c_str());
  if (!message_.empty()) ImGui::TextColored(kError, "%s", message_.c_str());
  ImGui::End();
}

void TitleMenu::drawMultiplayer(Choice &choice, uint16_t default_port) {
  beginPanel("##multiplayer", 520);
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
    ImGui::Text("%s  -  %s's game  (%d/%d)%s", b.world.c_str(), b.host.c_str(), b.players,
                b.max_players, !same ? "  different version" : full ? "  full" : "");
    ImGui::PopID();
  }

  ImGui::Spacing();
  ImGui::PushItemWidth(280);
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
  if (ImGui::Button("Back", ImVec2(-1, 0))) page = Page::Main;
  if (!status_.empty()) ImGui::TextUnformatted(status_.c_str());
  if (!message_.empty()) ImGui::TextColored(kError, "%s", message_.c_str());
  ImGui::End();
}
