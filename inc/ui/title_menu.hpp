#pragma once

#include "net/discovery.hpp"
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

// "host" or "host:port" as typed into the join box. Nothing if it isn't one.
struct HostAddress {
  std::string host;
  uint16_t port = 0;
};
std::optional<HostAddress> parseAddress(std::string_view text, uint16_t default_port);

// The screen shown at launch and after leaving a game: a name, then Single
// player / Host game / Join (games found on the network, or an address) /
// Quit. Drawn with ImGui; the Engine acts on what was chosen.
class TitleMenu {
public:
  struct Choice {
    enum Kind { None, Single, Host, Join, Quit } kind = None;
    HostAddress address; // Join
  };

  // Shown again (after a game, or at launch), with an optional message such
  // as why the last game ended.
  void open(std::string message = {});
  void close();
  // Draws the menu inside the current ImGui frame. `name` is edited in place.
  Choice draw(double now, std::string &name, uint16_t default_port);
  // A one-line status under the buttons ("Joining…").
  void setStatus(std::string status) { status_ = std::move(status); }

private:
  discovery::LanBrowser browser;
  bool browsing = false;
  std::string message_;
  std::string status_;
  char name_buf[32] = {};
  char address_buf[64] = {};
  bool synced_name = false;
};
