#pragma once

#include "camera.hpp"
#include "game_state.hpp"
#include "game_clock.hpp"
#include "shader.hpp"
#include "texture.hpp"
#include "world.hpp"
#include <GLFW/glfw3.h>
#include <glad/glad.h>
#include <string>

/**
 * engine.hpp
 *
 * The 3D game engine created by Will Hleucka
 *
 */
class Engine {
public:
  Engine(int w, int h, const std::string &t)
      : window(nullptr), width(w), height(h), title(t) {}

  Engine() { cleanup(); }

  bool init();
  void run();
  Shader *block_shader = nullptr;
  World *world = nullptr;
  Texture atlas_texture;
  bool show_stats = false;
  bool wireframe = false;

private:
  static void framebufferSizeCallback(GLFWwindow *window, int w, int h);
  void drawCrosshairImGui();
  void processInput();
  void loadAtlas(std::string path);
  void update();
  void render();
  void stats();
  void cleanup();

  GameState game_state = GameState::START;

  GLFWwindow *window;
  int width = 800, height = 600;
  std::string title;

  float last_frame = 0.0f;
  float delta_time = 0.0f;

  GameClock game_clock;
  Camera camera;
  float last_x = width / 2.0f;
  float last_y = height / 2.0f;
  bool first_mouse = true;
};
