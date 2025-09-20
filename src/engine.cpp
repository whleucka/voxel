#include "engine.hpp"
#include "block_data_manager.hpp"
#include "player.hpp"
#include "texture_manager.hpp"
#include <GLFW/glfw3.h>
#include <glm/ext/scalar_constants.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <sys/resource.h>

size_t getMemoryUsage() {
  struct rusage usage;
  getrusage(RUSAGE_SELF, &usage);
  // Defaults to KB (long)
  return usage.ru_maxrss / 1024.0f; // MB
}

void keyCallback(GLFWwindow *window, int key, int, int action,
                 int) {
  // This is better than processInput, because I don't have to worry about key
  // held vs key pressed, etc
  if (key == GLFW_KEY_F3 && action == GLFW_PRESS) {
    Engine *engine = 
        reinterpret_cast<Engine *>(glfwGetWindowUserPointer(window));
    engine->debug = !engine->debug;
  }
  if (key == GLFW_KEY_F4 && action == GLFW_PRESS) {
    Engine *engine = 
        reinterpret_cast<Engine *>(glfwGetWindowUserPointer(window));
    engine->wireframe = !engine->wireframe;
  }
}

Engine::Engine(int w, int h, const std::string &t)
    : window(nullptr), width(w), height(h), title(t) {}

Engine::~Engine() { cleanup(); }

bool Engine::init() {
  if (!glfwInit()) {
    std::cerr << "Failed to initialize GLFW\n";
    return false;
  }

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

  GLFWmonitor *primaryMonitor = glfwGetPrimaryMonitor();
  if (!primaryMonitor) {
    std::cerr << "Failed to get primary monitor\n";
    return false;
  }

  const GLFWvidmode *mode = glfwGetVideoMode(primaryMonitor);
  if (!mode) {
    std::cerr << "Failed to get video mode of primary monitor\n";
    return false;
  }

  width = mode->width;
  height = mode->height;
  last_x = width / 2.0f;
  last_y = height / 2.0f;

  window = 
      glfwCreateWindow(width, height, title.c_str(), primaryMonitor, nullptr);
  if (!window) {
    std::cerr << "Failed to create GLFW window\n";
    glfwTerminate();
    return false;
  }

  glfwMakeContextCurrent(window);
  glfwSetWindowUserPointer(window, this);
  glfwSetKeyCallback(window, keyCallback);

  if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
    std::cerr << "Failed to initialize GLAD\n";
    return false;
  }

  // Window resize
  glfwSetFramebufferSizeCallback(window, [](GLFWwindow *win, int w, int h) {
    auto *eng = static_cast<Engine *>(glfwGetWindowUserPointer(win));
    glViewport(0, 0, w, h); // <-- keep viewport in sync
    if (eng) {
      eng->width = w;
      eng->height = h;
    }
  });

  // Mouse
  glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
  glfwSetCursorPosCallback(
      window, [](GLFWwindow *win, double xpos, double ypos) {
        auto *eng = static_cast<Engine *>(glfwGetWindowUserPointer(win));
        if (!eng)
          return;
        if (eng->first_mouse) {
          eng->last_x = xpos;
          eng->last_y = ypos;
          eng->first_mouse = false;
        }
        float dx = float(xpos - eng->last_x);
        float dy = float(eng->last_y - ypos);
        eng->last_x = xpos;
        eng->last_y = ypos;
        eng->player->getCamera().processMouseMovement(dx, dy);
      });

  glfwSetMouseButtonCallback(
      window, [](GLFWwindow *win, int button, int action, int mods) {
        auto *eng = static_cast<Engine *>(glfwGetWindowUserPointer(win));
        if (!eng)
          return;
        eng->handleMouseClick(button, action, mods);
      });

  // OpenGL settings
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LESS);

  glEnable(GL_FRAMEBUFFER_SRGB);

  glEnable(GL_CULL_FACE);
  glCullFace(GL_BACK);
  glFrontFace(GL_CCW);

  // Don’t enable blending globally – only for the transparent pass.
  glDisable(GL_BLEND);

  glClearDepth(1.0);
  glDepthRange(0.0, 1.0);

  glfwSwapInterval(0); // 0 = disable vsync, 1 = enable vsync

  // ImGui
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO &io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

  ImGui::StyleColorsDark();

  // Platform/Renderer bindings
  ImGui_ImplGlfw_InitForOpenGL(window, true);
  ImGui_ImplOpenGL3_Init(
      "#version 330"); // or "#version 130" depending on context

  BlockDataManager::getInstance().load("res/block_data.json");

  renderer.init();
  loadAtlas("res/block_atlas.png");

  world.processUploads();

  int spawn_y = 100;
  player = new Player(&world, glm::vec3(0, spawn_y, 0));

  return true;
}

void Engine::run() {
  while (!glfwWindowShouldClose(window)) {
    float currentFrame = glfwGetTime();
    delta_time = currentFrame - last_frame;
    last_frame = currentFrame;

    processInput();
    update();
    render();

    glfwSwapBuffers(window);
    glfwPollEvents();
  }
}

void Engine::processInput() {
  // Close program
  if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
    glfwSetWindowShouldClose(window, true);

  bool forward = glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS;
  bool back = glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS;
  bool left = glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS;
  bool right = glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS;
  bool jump = glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS;
  bool sprint = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS;
  player->processKeyboard(forward, back, left, right, jump, sprint);
}

void Engine::loadAtlas(std::string path) {
  TextureManager::getInstance().loadTexture("block_atlas", path);
}

void Engine::update() {
  game_clock.update(delta_time);
  player->update(delta_time);
  world.update(player->getPosition());
  world.processUploads();

  if (wireframe) {
    glLineWidth(2.0f);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
  }
  else {
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
  }
}

void Engine::render() {
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  // Gather chunk pointers from world
  std::vector<Chunk *> visible_chunks = world.getVisibleChunks(player->getCamera(), width, height);

  // Set sky color based on time of day
  float time_fraction = game_clock.fractionOfDay();

  // Render scene
  renderer.draw(visible_chunks, player->getCamera(), width, height, time_fraction);

  imgui();
}

void Engine::drawCrosshairImGui() {
  ImVec2 center = ImGui::GetMainViewport()->GetCenter();
  ImDrawList *dl = ImGui::GetForegroundDrawList();

  float size = 10.0f;
  float thickness = 3.0f;
  ImU32 color = IM_COL32(255, 255, 255, 255);

  dl->AddLine(ImVec2(center.x - size, center.y),
              ImVec2(center.x + size, center.y), color, thickness);
  dl->AddLine(ImVec2(center.x, center.y - size),
              ImVec2(center.x, center.y + size), color, thickness);
}

void Engine::imgui() {
  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();
  if (debug) {
    // Per frame updates
    ImGui::Begin("Debug", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
    ImGuiIO &io = ImGui::GetIO();
    ImGui::Text("Game time: %.2d:%.2d", game_clock.hour(), game_clock.minute());
    ImGui::Text("FPS: %.1f", io.Framerate);
    ImGui::Text("Frame time: %.3f ms", 1000.0f / io.Framerate);
    const glm::vec3 pos = player->getPosition();
    ImGui::Text("Player: (%.2f, %.2f, %.2f)", pos.x, pos.y, pos.z);
    ImGui::Text("Memory usage: %zu MB", getMemoryUsage());
    ImGui::Text("Chunks loaded: %zu", world.getLoadedChunkCount());
    ImGui::Checkbox("Wireframe mode", &wireframe);
    ImGui::End();
  }
  drawCrosshairImGui();
  ImGui::Render();
  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void Engine::cleanup() {
  delete player;
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();

  if (window) {
    glfwDestroyWindow(window);
    glfwTerminate();
    window = nullptr;
  }
}

void Engine::handleMouseClick(int, int action, int) {
  if (action == GLFW_PRESS) {
  }
}

