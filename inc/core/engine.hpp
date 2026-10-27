#pragma once

#include "glad/glad.h"
#include "player/player.hpp"
#include "render/camera.hpp"
#include "core/session.hpp"
#include "ui/hud.hpp"
#include "ui/pause_menu.hpp"
#include "ui/title_menu.hpp"
#include "render/menu_scene.hpp"
#include "entity/entity_interp.hpp"
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
  Hud hud;
  EntityRenderer entity_renderer;
  EntityInterpolator entity_interp;
  double last_snapshot_time = 0.0;
  std::vector<EntityView> entity_views; // this frame's interpolated entities
  std::vector<PlayerView> player_views; // this frame's other players
  InventoryScreen inventory_screen;
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
  void setCursorCaptured(bool captured);
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
