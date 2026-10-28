#pragma once

#include "glad/glad.h"
#include "player/player.hpp"
#include "render/camera.hpp"
#include "core/session.hpp"
#include "ui/chat_view.hpp"
#include "ui/hud.hpp"
#include "ui/pause_menu.hpp"
#include "ui/title_menu.hpp"
#include "render/menu_scene.hpp"
#include "entity/entity_interp.hpp"
#include "render/block_outline.hpp"
#include "render/entity_renderer.hpp"
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <memory>
#include <string>

class Engine {
public:
  Engine(int w, int h, const std::string &t)
      : win_title(t), win_width(w), win_height(h) {};
  ~Engine();

  bool init();
  void run();

private:
  GLFWwindow *window = nullptr;
  std::string win_title;
  int win_width, win_height;
  int win_pos_x = 0, win_pos_y = 0;
  bool shown_vsync = false, shown_fullscreen = false; // what the window has now

  // The game being played (single player, hosted or joined); null in the menu.
  std::unique_ptr<Session> session;
  PlayerId player_id = 0;
  uint32_t respawn_count = 0;     // echoed in MovePlayer; see Respawned
  std::unique_ptr<Player> player; // the local player; created on Joined
  std::string last_rejection;     // latest CommandRejected, for the debug panel
  Inventory::Slots inventory_view{}; // the Game's latest InventoryContents
  GameMode game_mode = GameMode::Survival;   // the world's, from Joined
  PauseMenu pause;
  VitalsChanged vitals_view{Vitals::kMax, Vitals::kMax, Vitals::kMax, true, false};
  // In bed: the Game's latest SleepState, since when, and whether a key has
  // been let go since lying down (so the key that walked us there doesn't
  // get us straight up again) and LeaveBed sent.
  std::optional<SleepState> sleep_view;
  double sleep_since = 0.0;
  bool bed_keys_released = false, leave_sent = false;
  bool inBed() const { return sleep_view && sleep_view->in_bed; }
  Hud hud;
  EntityRenderer entity_renderer;
  BlockOutline block_outline;
  EntityInterpolator entity_interp;
  double last_snapshot_time = 0.0;
  std::vector<EntityView> entity_views; // this frame's interpolated entities
  std::vector<PlayerView> player_views; // this frame's other players
  // Our own fishing bobber in this frame's entities, if a line is out.
  const EntityView *myBobber() const {
    for (const EntityView &v : entity_views)
      if (v.kind == EntityKind::Bobber && v.owner == player_id) return &v;
    return nullptr;
  }
  InventoryScreen inventory_screen;
  SeenItems known_items;       // from KnownItems: what unlocks recipes
  bool known_baseline = true;  // the first KnownItems after joining: no popup
  int crafting_scroll = 0;     // first crafting row shown
  std::optional<ContainerView> container_view; // the furnace being looked into
  // Chat: what's been said, whether the panel is open (keys type into it),
  // whether it opened this frame, and what's typed so far.
  ChatLog chat_log;
  bool chat_open = false, chat_opening = false;
  char chat_buf[kChatBufBytes] = {};
  void setChatOpen(bool open);
  // Survival: the block the held left button is working on, and for how long.
  struct Mining { glm::ivec3 pos{0}; float seconds = 0.0f; float progress = 0.0f; };
  std::optional<Mining> mining;
  bool swinging = false; // this press hit a creature: holding it doesn't mine
  void updateMining(float dt);
  // The crafting panel's rows right now (inventory, discoveries, a table near).
  std::vector<CraftRow> craftingRowsNow();
  TitleMenu menu;
  MenuScene scene; // the live world behind the menu
  double join_started = 0.0; // a guest waiting for Joined since then

  bool first_mouse = true;
  float last_frame = 0.0f;
  float delta_time = 0.0f;
  float last_x;
  float last_y;


  static void keyCallback(GLFWwindow *window, int key, int, int action, int);
  static void framebufferSizeCallback(GLFWwindow *window, int width,
                                      int height);
  static void mouseCallback(GLFWwindow *window, double xpos, double ypos);
  static void mouseButtonCallback(GLFWwindow *window, int button, int action, int mods);
  static void scrollCallback(GLFWwindow *window, double xoffset, double yoffset);
  static void windowResize(GLFWwindow *window, int width, int height);
  World &world() { return session->world(); }
  Connection &connection() { return session->connection(); }
  // Starts playing in `s` with fresh client-side state.
  void startSession(std::unique_ptr<Session> s);
  // Saves and leaves the current game, back to the menu with `message`.
  void endSession(std::string message = {});
  bool playing() const { return session && player; }
  void menuFrame();
  bool paused() const { return pause.isOpen(); }
  void pauseAction(PauseMenu::Action action);
  void applyDisplaySettings();
  void startFromMenu(const TitleMenu::Choice &choice);
  // Shows or captures the cursor to match the game's state; once per frame.
  void updateCursor();
  void debugMultiplayer();
  void processInput(GLFWwindow *window);
  void update();
  void handleEvents();
  void selectSlot(int slot);
  void setInventoryOpen(bool open);
  void render();
  void renderCrosshair();
  void renderNameTags(const glm::mat4 &view_projection);
  void debug();
  void debugInventory();
  void cleanup();
};
