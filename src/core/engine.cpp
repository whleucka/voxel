#include "core/engine.hpp"
#include "core/constants.hpp"
#include "core/settings.hpp"
#include "render/renderer.hpp"
#include "imgui/backends/imgui_impl_glfw.h"
#include "imgui/backends/imgui_impl_opengl3.h"
#include "imgui/imgui.h"
#include "util/memory.hpp"
#include <GLFW/glfw3.h>
#include <glm/ext/scalar_constants.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <cstdio>
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
  glfwSetMouseButtonCallback(window, mouseButtonCallback);
  glfwSetScrollCallback(window, scrollCallback);
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

// ─── Input Processing (polled every frame) ─────────────────────────────

void Engine::processInput(GLFWwindow *window) {
  // Close program
  if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
    glfwSetWindowShouldClose(window, true);

  // Don't process movement when inventory is open
  auto& player = world.getPlayer();
  if (player->isInventoryOpen()) return;

  PlayerInput input;
  input.forward  = glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS;
  input.backward = glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS;
  input.left     = glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS;
  input.right    = glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS;
  input.jump     = glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS;
  input.sprint   = glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS;
  input.crouch   = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS;

  // In fly mode, space = up, shift = down
  if (player->isFlyMode()) {
    input.fly_up   = glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS;
    input.fly_down = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS;
  }

  player->processKeyboard(delta_time, input);
}

// ─── Update ────────────────────────────────────────────────────────────

void Engine::update() {
  game_clock.scale = g_settings.time_scale;
  game_clock.update(delta_time);
  world.update(delta_time);

  if (g_settings.wireframe) {
    glLineWidth(2.0f);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
  } else {
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
  }
}

// ─── Render ────────────────────────────────────────────────────────────

void Engine::render() {
  // Dynamic sky colour driven by time-of-day
  float tod = game_clock.fractionOfDay();
  glm::vec3 sky = Renderer::skyColor(tod);
  glClearColor(sky.r, sky.g, sky.b, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  // Render scene
  glm::mat4 view = world.getPlayer()->getCamera().getViewMatrix();
  glm::mat4 proj = glm::perspective(
      glm::radians(g_settings.fov),                  // field of view from settings
      (float)win_width / (float)win_height,           // aspect ratio
      0.1f,                                           // near plane
      1000.0f                                         // far plane
  );

  world.render(view, proj, tod);

  // ImGui frame must always be opened here so both renderCrosshair() and
  // debug() can safely submit widgets regardless of game state.
  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();

  renderCrosshair();
  debug();
}

// ─── Crosshair ─────────────────────────────────────────────────────────

void Engine::renderCrosshair() {
  // Don't show crosshair when inventory is open
  if (world.getPlayer()->isInventoryOpen()) return;

  // Draw crosshair at screen center using ImGui overlay
  ImDrawList* draw = ImGui::GetForegroundDrawList();
  float cx = win_width * 0.5f;
  float cy = win_height * 0.5f;
  float size = 10.0f;
  float thickness = 2.0f;
  ImU32 color = IM_COL32(255, 255, 255, 200);

  draw->AddLine(ImVec2(cx - size, cy), ImVec2(cx + size, cy), color, thickness);
  draw->AddLine(ImVec2(cx, cy - size), ImVec2(cx, cy + size), color, thickness);
}

// ─── Debug Panel ───────────────────────────────────────────────────────

void Engine::debug() {
  if (g_settings.show_debug) {
    auto& player = world.getPlayer();
    ImGui::Begin("Debug", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoNav);
    ImGuiIO &io = ImGui::GetIO();

    // Performance
    ImGui::Text("Game time: %.2d:%.2d", game_clock.hour(), game_clock.minute());

    static float fps_history[90] = {};
    static int   fps_idx = 0;
    fps_history[fps_idx % 90] = io.Framerate;
    ++fps_idx;
    char fps_overlay[32];
    std::snprintf(fps_overlay, sizeof(fps_overlay), "%.1f FPS", io.Framerate);
    ImGui::PlotLines("##fps", fps_history, 90, fps_idx % 90,
                     fps_overlay, 0.0f, 300.0f, ImVec2(0, 50));
    ImGui::Text("Frame time: %.3f ms", 1000.0f / io.Framerate);

    ImGui::Text("Memory usage: %zu MB", getMemoryUsage());
    ImGui::Text("Chunks loaded: %zu", world.getChunkCount());

    ImGui::Separator();

    // Player info
    glm::vec3 pos = player->getPosition();
    glm::vec3 vel = player->getVelocity();
    ImGui::Text("Position: (%.2f, %.2f, %.2f)", pos.x, pos.y, pos.z);
    ImGui::Text("Velocity: (%.2f, %.2f, %.2f)", vel.x, vel.y, vel.z);
    ImGui::Text("On ground: %s", player->isOnGround() ? "yes" : "no");
    ImGui::Text("Underwater: %s", player->isUnderwater() ? "yes" : "no");
    ImGui::Text("Sprinting: %s", player->isSprinting() ? "yes" : "no");
    ImGui::Text("Crouching: %s", player->isCrouching() ? "yes" : "no");
    ImGui::Text("Hotbar slot: %d", player->getSelectedHotbarSlot() + 1);

    // Raycast info
    const Camera& cam = player->getCamera();
    RaycastResult ray = world.raycast(cam.position, cam.front, kPlayerReach);
    if (ray.hit) {
      ImGui::Text("Looking at: (%d, %d, %d) [%s]",
                  ray.block_pos.x, ray.block_pos.y, ray.block_pos.z,
                  "solid");
      ImGui::Text("Face normal: (%d, %d, %d)", ray.normal.x, ray.normal.y, ray.normal.z);
      ImGui::Text("Distance: %.2f", ray.distance);
    } else {
      ImGui::Text("Looking at: (none)");
    }

    ImGui::Separator();

    // Settings
    ImGui::Checkbox("Fly mode", player->getFlyModePtr());
    ImGui::Checkbox("Wireframe mode", &g_settings.wireframe);
    ImGui::Checkbox("VSync", &g_settings.vsync);
    ImGui::Checkbox("Fullscreen", &g_settings.fullscreen);

    ImGui::Separator();

    // Adjustable settings
    ImGui::SliderFloat("Mouse sensitivity", &g_settings.mouse_sensitivity, 0.01f, 0.30f, "%.3f");
    ImGui::SliderFloat("FOV", &g_settings.fov, 30.0f, 110.0f, "%.0f");

    ImGui::Separator();

    // Lighting & atmosphere
    ImGui::Text("Atmosphere");
    ImGui::SliderFloat("Time scale", &g_settings.time_scale, 0.0f, 720.0f, "%.0f");
    ImGui::SliderFloat("Fog start",  &g_settings.fog_start,  0.0f, 300.0f, "%.0f");
    ImGui::SliderFloat("Fog end",    &g_settings.fog_end,    0.0f, 400.0f, "%.0f");

    ImGui::End();
  }
  ImGui::Render();
  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

// ─── Key Callback (event-based, for toggle actions) ────────────────────

void Engine::keyCallback(GLFWwindow *window, int key, int, int action, int) {
  Engine *engine =
      reinterpret_cast<Engine *>(glfwGetWindowUserPointer(window));

  if (action != GLFW_PRESS) return;

  switch (key) {
    // Debug toggles
    case GLFW_KEY_F1:
      g_settings.show_debug = !g_settings.show_debug;
      break;
    case GLFW_KEY_F2:
      g_settings.wireframe = !g_settings.wireframe;
      break;
    case GLFW_KEY_F3:
      g_settings.vsync = !g_settings.vsync;
      glfwSwapInterval(g_settings.vsync ? 1 : 0);
      break;

    // Fly mode toggle
    case GLFW_KEY_T:
      engine->world.getPlayer()->toggleFlyMode();
      break;

    // Inventory toggle
    case GLFW_KEY_E: {
      auto& player = engine->world.getPlayer();
      player->toggleInventory();
      if (player->isInventoryOpen()) {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
      } else {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        engine->first_mouse = true; // prevent mouse jump when closing inventory
      }
      break;
    }

    // Hotbar slots 1-9
    case GLFW_KEY_1: engine->world.getPlayer()->setSelectedHotbarSlot(0); break;
    case GLFW_KEY_2: engine->world.getPlayer()->setSelectedHotbarSlot(1); break;
    case GLFW_KEY_3: engine->world.getPlayer()->setSelectedHotbarSlot(2); break;
    case GLFW_KEY_4: engine->world.getPlayer()->setSelectedHotbarSlot(3); break;
    case GLFW_KEY_5: engine->world.getPlayer()->setSelectedHotbarSlot(4); break;
    case GLFW_KEY_6: engine->world.getPlayer()->setSelectedHotbarSlot(5); break;
    case GLFW_KEY_7: engine->world.getPlayer()->setSelectedHotbarSlot(6); break;
    case GLFW_KEY_8: engine->world.getPlayer()->setSelectedHotbarSlot(7); break;
    case GLFW_KEY_9: engine->world.getPlayer()->setSelectedHotbarSlot(8); break;

    // Fullscreen toggle
    case GLFW_KEY_F11: {
      g_settings.fullscreen = !g_settings.fullscreen;
      if (g_settings.fullscreen) {
        glfwGetWindowPos(window, &engine->win_pos_x, &engine->win_pos_y);
        glfwGetWindowSize(window, &engine->win_width, &engine->win_height);
        GLFWmonitor *primaryMonitor = glfwGetPrimaryMonitor();
        const GLFWvidmode *mode = glfwGetVideoMode(primaryMonitor);
        glfwSetWindowMonitor(window, primaryMonitor, 0, 0, mode->width,
                             mode->height, mode->refreshRate);
      } else {
        glfwSetWindowMonitor(window, nullptr, engine->win_pos_x,
                             engine->win_pos_y, engine->win_width,
                             engine->win_height, 0);
      }
      break;
    }

    default:
      break;
  }
}

// ─── Mouse Move Callback ───────────────────────────────────────────────

void Engine::mouseCallback(GLFWwindow *window, double xpos, double ypos) {
  Engine *engine = reinterpret_cast<Engine *>(glfwGetWindowUserPointer(window));

  // Don't process mouse look when inventory is open
  if (engine->world.getPlayer()->isInventoryOpen()) return;

  if (engine->first_mouse) {
    engine->last_x = xpos;
    engine->last_y = ypos;
    engine->first_mouse = false;
  }

  float xoffset = xpos - engine->last_x;
  float yoffset = engine->last_y - ypos; // inverted Y

  engine->last_x = xpos;
  engine->last_y = ypos;

  engine->world.getPlayer()->processMouseMovement(xoffset, yoffset);
}

// ─── Mouse Button Callback ─────────────────────────────────────────────

void Engine::mouseButtonCallback(GLFWwindow *window, int button, int action, int) {
  Engine *engine = reinterpret_cast<Engine *>(glfwGetWindowUserPointer(window));

  // Don't process when inventory is open
  if (engine->world.getPlayer()->isInventoryOpen()) return;

  if (action != GLFW_PRESS) return;

  auto& player = engine->world.getPlayer();
  const Camera& cam = player->getCamera();
  RaycastResult ray = engine->world.raycast(cam.position, cam.front, kPlayerReach);

  if (button == GLFW_MOUSE_BUTTON_LEFT) {
    // Destroy block (left click)
    if (ray.hit) {
      // TODO: implement block destruction via world.setBlockAt()
      // For now, just detected -- will be implemented with world modification
    }
  } else if (button == GLFW_MOUSE_BUTTON_RIGHT) {
    // Place block / use item (right click)
    if (ray.hit) {
      // Placement position would be: ray.block_pos + ray.normal
      // TODO: implement block placement via world.setBlockAt()
    }
  }
}

// ─── Scroll Callback (hotbar slot cycling) ─────────────────────────────

void Engine::scrollCallback(GLFWwindow *window, double, double yoffset) {
  Engine *engine = reinterpret_cast<Engine *>(glfwGetWindowUserPointer(window));
  auto& player = engine->world.getPlayer();

  int slot = player->getSelectedHotbarSlot();
  if (yoffset < 0) {
    slot = (slot + 1) % kHotbarSlots;
  } else if (yoffset > 0) {
    slot = (slot - 1 + kHotbarSlots) % kHotbarSlots;
  }
  player->setSelectedHotbarSlot(slot);
}

// ─── Window Resize ─────────────────────────────────────────────────────

void Engine::windowResize(GLFWwindow *window, int w, int h) {
  Engine *engine = reinterpret_cast<Engine *>(glfwGetWindowUserPointer(window));
  glViewport(0, 0, w, h);
  engine->win_width = w;
  engine->win_height = h;
}

// ─── Cleanup ───────────────────────────────────────────────────────────

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
