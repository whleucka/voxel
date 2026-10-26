#include "ui/title_menu.hpp"
#include "net/wire.hpp"
#include "sim/game.hpp"
#include "imgui/imgui.h"
#include <charconv>
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

void TitleMenu::open(std::string message) {
  message_ = std::move(message);
  status_.clear();
  synced_name = false;
  std::string error;
  browsing = browser.start(error);
}

void TitleMenu::close() {
  browser = discovery::LanBrowser{}; // stop listening
  browsing = false;
}

TitleMenu::Choice TitleMenu::draw(double now, std::string &name, uint16_t default_port) {
  if (!synced_name) {
    std::snprintf(name_buf, sizeof name_buf, "%s", name.c_str());
    synced_name = true;
  }
  if (browsing) browser.poll(now);

  Choice choice;
  const ImGuiIO &io = ImGui::GetIO();
  ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f),
                          ImGuiCond_Always, ImVec2(0.5f, 0.5f));
  ImGui::SetNextWindowSize(ImVec2(440, 0), ImGuiCond_Always);
  ImGui::Begin("Voxel", nullptr,
               ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                   ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings);

  ImGui::SetWindowFontScale(1.4f);
  ImGui::TextUnformatted("Voxel");
  ImGui::SetWindowFontScale(1.0f);
  ImGui::Separator();

  ImGui::InputText("Your name", name_buf, sizeof name_buf);
  name = name_buf;
  bool name_ok = Game::validName(name);
  if (!name_ok)
    ImGui::TextColored(ImVec4(1.f, 0.6f, 0.3f, 1.f), "1-16 letters, digits, _ or -");

  ImGui::Spacing();
  ImGui::BeginDisabled(!name_ok);
  ImVec2 wide(-1, 0);
  if (ImGui::Button("Single player", wide)) choice.kind = Choice::Single;
  if (ImGui::Button("Host game", wide)) choice.kind = Choice::Host;

  ImGui::Spacing();
  ImGui::SeparatorText("Join a game");
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
    if (ImGui::Button("Join")) {
      choice.kind = Choice::Join;
      choice.address = {g.address, b.port};
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::Text("%s  -  %s's game  (%d/%d)%s", b.world.c_str(), b.host.c_str(), b.players,
                b.max_players, !same ? "  different version" : full ? "  full" : "");
    ImGui::PopID();
  }

  ImGui::Spacing();
  ImGui::PushItemWidth(260);
  bool enter = ImGui::InputTextWithHint("##address", "address, e.g. 192.168.1.20",
                                        address_buf, sizeof address_buf,
                                        ImGuiInputTextFlags_EnterReturnsTrue);
  ImGui::PopItemWidth();
  ImGui::SameLine();
  auto typed = parseAddress(address_buf, default_port);
  ImGui::BeginDisabled(!typed);
  if ((ImGui::Button("Join address") || enter) && typed) {
    choice.kind = Choice::Join;
    choice.address = *typed;
  }
  ImGui::EndDisabled();
  ImGui::EndDisabled(); // name_ok

  ImGui::Spacing();
  ImGui::Separator();
  if (ImGui::Button("Quit", wide)) choice.kind = Choice::Quit;

  if (!status_.empty()) ImGui::TextUnformatted(status_.c_str());
  if (!message_.empty())
    ImGui::TextColored(ImVec4(1.f, 0.45f, 0.4f, 1.f), "%s", message_.c_str());
  ImGui::End();
  return choice;
}
