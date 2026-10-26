#pragma once

#include "glad/glad.h"
#include "player/player.hpp"
#include "render/camera.hpp"
#include "sim/game.hpp"
#include "sim/local_connection.hpp"
#include "ui/hud.hpp"
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
  int win_pos_x, win_pos_y;

  Game game;
  LocalConnection connection;
  PlayerId player_id = 0;
  std::unique_ptr<Player> player; // the local player; created on Joined
  std::string last_rejection;     // latest CommandRejected, for the debug panel
  Inventory::Slots inventory_view{}; // the Game's latest InventoryContents
  VitalsChanged vitals_view{Vitals::kMax, Vitals::kMax, Vitals::kMax, true, false};
  Hud hud;
  EntityRenderer entity_renderer;
  EntityInterpolator entity_interp;
  double last_snapshot_time = 0.0;
  std::vector<EntityView> entity_views; // this frame's interpolated entities
  InventoryScreen inventory_screen;

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
  void processInput(GLFWwindow *window);
  void update();
  void handleEvents();
  void selectSlot(int slot);
  void setInventoryOpen(bool open);
  void render();
  void renderCrosshair();
  void debug();
  void debugInventory();
  void cleanup();
};
