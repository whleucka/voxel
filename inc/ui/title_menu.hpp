#pragma once

#include "net/discovery.hpp"
#include "world/world_list.hpp"
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// "host" or "host:port" as typed into the join box. Nothing if it isn't one.
struct HostAddress {
  std::string host;
  uint16_t port = 0;
};
std::optional<HostAddress> parseAddress(std::string_view text, uint16_t default_port);

// "512 B", "48 KB", "3.4 MB": a world's size in the list.
std::string sizeText(uintmax_t bytes);

// The screen shown at launch and after leaving a game, over the live scene.
//   Main:          your name; Single player, Multiplayer, Quit.
//   Single player: the world list; Play, Host on network, Create, Rename,
//                  Delete (with a confirm step), Back.
//   Multiplayer:   games found on the network, an address box, Back.
// Drawn with ImGui; the Engine acts on what was chosen. World files are
// managed here directly (they're only folders under the saves root).
class TitleMenu {
public:
  struct Choice {
    enum Kind { None, Play, Host, Join, Quit } kind = None;
    std::string world;   // Play, Host
    HostAddress address; // Join
  };

  explicit TitleMenu(std::filesystem::path saves_root = "saves") : root(std::move(saves_root)) {}

  // Shown again (after a game, or at launch), with an optional message such
  // as why the last game ended.
  void open(std::string message = {});
  void close();
  // Draws the menu inside the current ImGui frame. `name` (the player's) and
  // `last_world` (the world to select; follows a rename) are edited in place.
  Choice draw(double now, std::string &name, std::string &last_world, uint16_t default_port);
  // A one-line status ("Joining…").
  void setStatus(std::string status) { status_ = std::move(status); }

private:
  enum class Page { Main, Worlds, Multiplayer };
  enum class Form { None, Create, Rename, Delete };

  void drawTitle();
  void drawMain(Choice &choice, std::string &name);
  void drawWorlds(Choice &choice, std::string &last_world);
  void drawMultiplayer(Choice &choice, uint16_t default_port);
  void refreshWorlds(const std::string &select);

  std::filesystem::path root;
  Page page = Page::Main;
  Form form = Form::None;
  std::vector<WorldInfo> worlds;
  int selected = -1;
  discovery::LanBrowser browser;
  bool browsing = false;
  std::string message_;
  std::string status_;
  char name_buf[32] = {};
  char address_buf[64] = {};
  char world_buf[40] = {};
  char seed_buf[64] = {};
  bool synced_name = false;
};
