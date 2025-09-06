#pragma once

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <string>

enum GameState {
  INTRO = 0,
  MENU,
  START,
  GAME_OVER,
};

class Engine {
public:
  Engine(int w, int h, const std::string &title);
  ~Engine();

  bool init();
  void run();

private:
  GameState gameState = GameState::START;

  void processInput();
  void update(float deltaTime);
  void render();
  void cleanup();
  static void framebufferSizeCallback(GLFWwindow *window, int w, int h);

  GLFWwindow *window;
  int width, height;
  std::string title;

  float lastFrame = 0.0f;
  float deltaTime = 0.0f;
};
