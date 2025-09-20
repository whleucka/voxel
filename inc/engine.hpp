#pragma once

#include "camera.hpp"
#include "game_clock.hpp"
#include "renderer.hpp"
#include "world.hpp"
#include <GLFW/glfw3.h>
#include <glad/glad.h>
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

  static void framebufferSizeCallback(GLFWwindow *window, int w, int h);
  void drawCrosshairImGui();
  void processInput();
  void loadAtlas(std::string path);
  void update();
  void render();
  void imgui();
  void cleanup();
  void handleMouseClick(int button, int action, int mods);
  void checkOpenGLError(const char* stmt, const char* fname, int line);
};
