#pragma once

#include "camera.hpp"
#include "game_clock.hpp"
#include "renderer.hpp"
#include "world.hpp"
#include <GLFW/glfw3.h>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <string>

class Player;

enum GameMode {
  SURVIVAL = 0,
  CREATIVE,
};

class Engine {
public:
  Engine(int w, int h, const std::string &t);
  ~Engine();

  bool init();
  void run();
  bool debug = false;
  bool wireframe = false;

private:
  GLFWwindow *window;
  int width, height;
  std::string title;

  bool spawn = false;
  float last_frame = 0.0f;
  float delta_time = 0.0f;
  float last_x;
  float last_y;
  bool first_mouse = true;

  GameMode mode = GameMode::SURVIVAL;

  GameClock game_clock;
  Player *player;
  World world;
  Renderer renderer;

  bool has_highlighted_block = false;
  glm::vec3 highlighted_block_pos;

  glm::vec3 cloud_offset = glm::vec3(0.0f);

  static void framebufferSizeCallback(GLFWwindow *window, int w, int h);
  void drawCrosshairImGui();
  void processInput();
  void loadAtlas(std::string path);
  void update();
  void render();
  void imgui();
  void cleanup();
  void handleMouseClick(int button, int action, int mods);
  void checkOpenGLError(const char *stmt, const char *fname, int line);
};
