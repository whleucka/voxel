#pragma once

#include "camera.hpp"
#include "game_clock.hpp"
#include "renderer.hpp"
#include "world.hpp"
#include "cloud_manager.hpp"
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

  void handleKeyPress(int key, int action);

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
  CloudManager cloud_manager;

  bool has_highlighted_block = false;
  glm::vec3 highlighted_block_pos;
  BlockType selected_block_type = BlockType::STONE;

  void changeSelectedBlockType(BlockType type);
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
