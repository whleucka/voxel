#include "core/engine.hpp"
#include "core/settings.hpp"
#include "imgui/backends/imgui_impl_glfw.h"
#include "imgui/backends/imgui_impl_opengl3.h"
#include "imgui/imgui.h"
#include "util/memory.hpp"
#include <GLFW/glfw3.h>
#include <glm/ext/scalar_constants.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>

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

  win_width = mode->width;
  win_height = mode->height;
  last_x = win_width / 2.0f;
  last_y = win_height / 2.0f;

  window = glfwCreateWindow(win_width, win_height, win_title.c_str(),
                            primaryMonitor, nullptr);
  if (!window) {
    std::cerr << "Failed to create GLFW window\n";
    glfwTerminate();
    return false;
  }

  glfwMakeContextCurrent(window);
  glfwSetWindowUserPointer(window, this);
  glfwSetKeyCallback(window, keyCallback);
  glfwSetCursorPosCallback(window, mouseCallback);
  glfwSetFramebufferSizeCallback(window, windowResize);
  glfwSetInputMode(window, GLFW_CURSOR,
                   g_settings.show_cursor ? GLFW_CURSOR_NORMAL
                                          : GLFW_CURSOR_DISABLED);

  if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
    std::cerr << "Failed to initialize GLAD\n";
    return false;
  }

  // OpenGL settings
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LESS);
  glEnable(GL_FRAMEBUFFER_SRGB);
  glEnable(GL_CULL_FACE);
  glCullFace(GL_BACK);
  glFrontFace(GL_CCW);
  glDisable(GL_BLEND);
  glClearDepth(1.0);
  glDepthRange(0.0, 1.0);
  glViewport(0, 0, win_width, win_height);

  glfwSwapInterval(g_settings.vsync ? 1 : 0);

  // ImGui
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO &io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

  ImGui::StyleColorsDark();

  // Platform/Renderer bindings
  ImGui_ImplGlfw_InitForOpenGL(window, true);
  ImGui_ImplOpenGL3_Init("#version 330");

  return true;
}

void Engine::run() {
  world.init();

  while (!glfwWindowShouldClose(window)) {
    float currentFrame = glfwGetTime();
    delta_time = currentFrame - last_frame;
    last_frame = currentFrame;

    processInput(window);
    update();
    render();

    glfwSwapBuffers(window);
    glfwPollEvents();
  }
}

void Engine::processInput(GLFWwindow *window) {
  // Close program
  if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
    glfwSetWindowShouldClose(window, true);

  world.getPlayer()->processKeyboard(
      delta_time, glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS,
      glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS,
      glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS,
      glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS,
      glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS,
      glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS,
      glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS,
      glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS);
}

void Engine::update() {
  game_clock.update(delta_time);
  world.update(delta_time);

  if (g_settings.wireframe) {
    glLineWidth(2.0f);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
  } else {
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
  }
}

void Engine::render() {

  glClearColor(0.53f, 0.81f, 0.92f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  // Render scene
  glm::mat4 view = world.getPlayer()->getCamera().getViewMatrix();
  glm::mat4 proj = glm::perspective(
      glm::radians(world.getPlayer()->getCamera().zoom), // field of view
      (float)win_width / (float)win_height,              // aspect ratio
      0.1f,                                              // near plane
      1000.0f                                            // far plane
  );

  world.render(view, proj);
  debug();
}

void Engine::debug() {
  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();
  if (g_settings.show_debug) {
    // Per frame updates
    ImGui::Begin("Debug", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
    ImGuiIO &io = ImGui::GetIO();
    ImGui::Text("Game time: %.2d:%.2d", game_clock.hour(), game_clock.minute());
    ImGui::Text("FPS: %.1f", io.Framerate);
    ImGui::Text("Frame time: %.3f ms", 1000.0f / io.Framerate);
    ImGui::Text("Memory usage: %zu MB", getMemoryUsage());
    ImGui::Text("Chunks loaded: %zu", world.getChunkCount());
    glm::vec3 pos = world.getPlayer()->getPosition();
    ImGui::Text("Player: (%.2f, %.2f, %.2f)", pos.x, pos.y, pos.z);
    ImGui::Checkbox("Fly mode", world.getPlayer()->getFlyModePtr());
    ImGui::Checkbox("Wireframe mode", &g_settings.wireframe);
    ImGui::Checkbox("VSync", &g_settings.vsync);
    ImGui::Checkbox("Fullscreen", &g_settings.fullscreen);
    ImGui::End();
  }
  ImGui::Render();
  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void Engine::keyCallback(GLFWwindow *window, int key, int, int action, int) {
  if (key == GLFW_KEY_F1 && action == GLFW_PRESS) {
    g_settings.show_debug = !g_settings.show_debug;
  }
  if (key == GLFW_KEY_F2 && action == GLFW_PRESS) {
    g_settings.wireframe = !g_settings.wireframe;
  }
  if (key == GLFW_KEY_F3 && action == GLFW_PRESS) {
    g_settings.vsync = !g_settings.vsync;
    glfwSwapInterval(g_settings.vsync ? 1 : 0);
  }
  if (key == GLFW_KEY_T && action == GLFW_PRESS) {
    Engine *engine =
        reinterpret_cast<Engine *>(glfwGetWindowUserPointer(window));
    engine->world.getPlayer()->toggleFlyMode();
  }
  if (key == GLFW_KEY_F11 && action == GLFW_PRESS) {
    Engine *engine =
        reinterpret_cast<Engine *>(glfwGetWindowUserPointer(window));
    g_settings.fullscreen = !g_settings.fullscreen;
    if (g_settings.fullscreen) {
      // Store current window position and size
      glfwGetWindowPos(window, &engine->win_pos_x, &engine->win_pos_y);
      glfwGetWindowSize(window, &engine->win_width, &engine->win_height);

      // Get primary monitor and its video mode
      GLFWmonitor *primaryMonitor = glfwGetPrimaryMonitor();
      const GLFWvidmode *mode = glfwGetVideoMode(primaryMonitor);

      // Go fullscreen
      glfwSetWindowMonitor(window, primaryMonitor, 0, 0, mode->width,
                           mode->height, mode->refreshRate);
    } else {
      // Restore windowed mode
      glfwSetWindowMonitor(window, nullptr, engine->win_pos_x,
                           engine->win_pos_y, engine->win_width,
                           engine->win_height, 0);
    }
  }
}

void Engine::mouseCallback(GLFWwindow *window, double xpos, double ypos) {
  Engine *engine = reinterpret_cast<Engine *>(glfwGetWindowUserPointer(window));
  if (engine->first_mouse) {
    engine->last_x = xpos;
    engine->last_y = ypos;
    engine->first_mouse = false;
  }

  float xoffset = xpos - engine->last_x;
  float yoffset = engine->last_y - ypos;

  engine->last_x = xpos;
  engine->last_y = ypos;

  engine->world.getPlayer()->processMouseMovement(xoffset, yoffset);
}

void Engine::windowResize(GLFWwindow *window, int w, int h) {
  Engine *engine = reinterpret_cast<Engine *>(glfwGetWindowUserPointer(window));
  glViewport(0, 0, w, h);
  engine->win_width = w;
  engine->win_height = h;
}

void Engine::cleanup() {
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();

  if (window) {
    glfwDestroyWindow(window);
    glfwTerminate();
    window = nullptr;
  }
}
