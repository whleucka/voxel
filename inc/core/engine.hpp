#pragma once

#include "glad/glad.h"
#include "render/camera.hpp"
#include "world/game_clock.hpp"
#include "world/world.hpp"
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <string>

class Engine {
public:
  Engine(int w, int h, const std::string &t)
      : win_title(t), win_width(w), win_height(h) {};
  ~Engine();

  bool init();
  void run();
  void saveGame(); // persist world + player to the active save directory

private:
  GLFWwindow *window = nullptr;
  std::string win_title;
  int win_width, win_height;
  int win_pos_x, win_pos_y;

  World world;
  std::string save_dir; // saves/<world_name>, resolved at run() start

  bool first_mouse = true;
  float last_frame = 0.0f;
  float delta_time = 0.0f;
  float last_x;
  float last_y;

  GameClock game_clock;

  static void keyCallback(GLFWwindow *window, int key, int, int action, int);
  static void framebufferSizeCallback(GLFWwindow *window, int width,
                                      int height);
  static void mouseCallback(GLFWwindow *window, double xpos, double ypos);
  static void mouseButtonCallback(GLFWwindow *window, int button, int action, int mods);
  static void scrollCallback(GLFWwindow *window, double xoffset, double yoffset);
  static void windowResize(GLFWwindow *window, int width, int height);
  void processInput(GLFWwindow *window);
  void update();
  void render();
  void renderCrosshair();
  void debug();
  void cleanup();
};
