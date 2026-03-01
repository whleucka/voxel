#include "core/engine.hpp"
#include "block/block_data.hpp"
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
#include <cstdlib>
#include <ctime>
#include <iostream>

Engine::~Engine() { cleanup(); }

bool Engine::init() {
  // Seed the world using current time so every launch is unique.
  g_settings.world_seed = static_cast<uint32_t>(std::time(nullptr));
  srand(g_settings.world_seed);
  // Offset noise coordinates to get a unique world each seed.
  // Capped at 5000 units — large enough for variety, small enough to avoid
  // degenerate all-ocean regions that appear at extreme offsets.
  g_settings.noise_offset = glm::vec2(
      static_cast<float>(g_settings.world_seed % 5000),
      static_cast<float>((g_settings.world_seed / 5000) % 5000)
  );
  std::cout << "World seed: " << g_settings.world_seed << "\n";

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
  glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  // Render scene
  glm::mat4 view = world.getPlayer()->getCamera().getViewMatrix();
  glm::mat4 proj = glm::perspective(
      glm::radians(g_settings.fov),                  // field of view from settings
      (float)win_width / (float)win_height,           // aspect ratio
      0.01f,                                          // near plane (small = less wall clipping in tight spaces)
      1000.0f                                         // far plane
  );

  world.render(view, proj, game_clock.fractionOfDay(), win_width, win_height);

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

  // Dark overlay when the camera eye is inside a solid block (e.g. clipping into a wall)
  {
    glm::vec3 eyePos = world.getPlayer()->getEyePosition();
    BlockType eyeBlock = world.getBlockAt(eyePos);
    if (eyeBlock != BlockType::AIR && !isLiquid(eyeBlock)) {
      ImDrawList* overlay = ImGui::GetForegroundDrawList();
      overlay->AddRectFilled(ImVec2(0, 0), ImVec2((float)win_width, (float)win_height),
                             IM_COL32(0, 0, 0, 200));
    }
  }

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

// ─── Debug / Settings Panel ────────────────────────────────────────────

void Engine::debug() {
  if (g_settings.show_debug) {
    auto& player = world.getPlayer();
    ImGuiIO& io  = ImGui::GetIO();

    ImGui::SetNextWindowSize(ImVec2(360, 0), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
    ImGui::Begin("Voxel Engine", nullptr,
                 ImGuiWindowFlags_NoNav | ImGuiWindowFlags_AlwaysAutoResize);

    if (ImGui::BeginTabBar("##tabs")) {

      // ── Debug tab ────────────────────────────────────────────────────
      if (ImGui::BeginTabItem("Debug")) {

        // Performance
        if (ImGui::CollapsingHeader("Performance", ImGuiTreeNodeFlags_DefaultOpen)) {
          static float fps_history[90] = {};
          static int   fps_idx = 0;
          fps_history[fps_idx % 90] = io.Framerate;
          ++fps_idx;
          char fps_overlay[32];
          std::snprintf(fps_overlay, sizeof(fps_overlay), "%.1f FPS", io.Framerate);
          ImGui::PushItemWidth(-1);
          ImGui::PlotLines("##fps", fps_history, 90, fps_idx % 90,
                           fps_overlay, 0.0f, 300.0f, ImVec2(0, 50));
          ImGui::PopItemWidth();
          ImGui::Text("Frame time : %.3f ms", 1000.0f / io.Framerate);
          ImGui::Text("Memory     : %zu MB",  getMemoryUsage());
          ImGui::Text("Chunks     : %zu",     world.getChunkCount());
          ImGui::Text("Game time  : %02d:%02d", game_clock.hour(), game_clock.minute());
        }

        // Player
        if (ImGui::CollapsingHeader("Player", ImGuiTreeNodeFlags_DefaultOpen)) {
          glm::vec3 pos = player->getPosition();
          glm::vec3 vel = player->getVelocity();
          ImGui::Text("Pos  (%.1f, %.1f, %.1f)", pos.x, pos.y, pos.z);
          ImGui::Text("Vel  (%.1f, %.1f, %.1f)", vel.x, vel.y, vel.z);

          // Compact flag row
          auto flag = [](const char* label, bool v) {
            ImGui::TextDisabled("%s", label);
            ImGui::SameLine();
            ImGui::TextColored(v ? ImVec4(0.4f,1.f,0.4f,1.f)
                                 : ImVec4(0.5f,0.5f,0.5f,1.f),
                               v ? "yes" : "no");
          };
          flag("Ground",    player->isOnGround());   ImGui::SameLine(0, 12);
          flag("Water",     player->isUnderwater());
          flag("Sprint",    player->isSprinting());  ImGui::SameLine(0, 12);
          flag("Crouch",    player->isCrouching());
          ImGui::Text("Hotbar slot : %d [%.*s]",
                      player->getSelectedHotbarSlot() + 1,
                      static_cast<int>(blockName(player->getHotbarBlock()).size()),
                      blockName(player->getHotbarBlock()).data());
        }

        // World / Raycast
        if (ImGui::CollapsingHeader("World")) {
          const Camera&   cam = player->getCamera();
          RaycastResult   ray = world.raycast(cam.position, cam.front, kPlayerReach);
          if (ray.hit) {
            ImGui::Text("Block  (%d, %d, %d)",
                        ray.block_pos.x, ray.block_pos.y, ray.block_pos.z);
            ImGui::Text("Normal (%d, %d, %d)",
                        ray.normal.x, ray.normal.y, ray.normal.z);
            ImGui::Text("Dist   %.2f", ray.distance);
          } else {
            ImGui::TextDisabled("Not looking at a block");
          }
        }

        ImGui::EndTabItem();
      }

      // ── Settings tab ─────────────────────────────────────────────────
      if (ImGui::BeginTabItem("Settings")) {

        // Controls
        if (ImGui::CollapsingHeader("Controls", ImGuiTreeNodeFlags_DefaultOpen)) {
          ImGui::Checkbox("Fly mode", player->getFlyModePtr());
          ImGui::PushItemWidth(180);
          ImGui::SliderFloat("Sensitivity", &g_settings.mouse_sensitivity,
                             0.01f, 0.30f, "%.3f");
          ImGui::SliderFloat("FOV",         &g_settings.fov,
                             30.0f, 110.0f, "%.0f deg");
          ImGui::PopItemWidth();
        }

        // Rendering
        if (ImGui::CollapsingHeader("Rendering", ImGuiTreeNodeFlags_DefaultOpen)) {
          ImGui::Checkbox("VSync",      &g_settings.vsync);
          ImGui::SameLine();
          ImGui::Checkbox("Wireframe",  &g_settings.wireframe);
          ImGui::SameLine();
          ImGui::Checkbox("Fullscreen", &g_settings.fullscreen);

          ImGui::PushItemWidth(180);
          ImGui::SliderFloat("Fog start", &g_settings.fog_start,  0.f, 300.f, "%.0f");
          ImGui::SliderFloat("Fog end",   &g_settings.fog_end,    0.f, 400.f, "%.0f");
          ImGui::PopItemWidth();
        }

        // Atmosphere
        if (ImGui::CollapsingHeader("Atmosphere", ImGuiTreeNodeFlags_DefaultOpen)) {
          // Time of day: read/write directly on game_clock (in hours 0–24)
          float tod_hours = game_clock.time_of_day / 3600.0f;
          ImGui::PushItemWidth(180);
          if (ImGui::SliderFloat("Time of day", &tod_hours, 0.0f, 24.0f, "%.1f h"))
            game_clock.time_of_day = tod_hours * 3600.0f;

          ImGui::SliderFloat("Time scale",       &g_settings.time_scale,
                             0.0f, 720.0f, "%.0f");
          ImGui::SliderFloat("Water fog",        &g_settings.water_fog_density,
                             0.0f, 0.20f,  "%.3f");
          ImGui::PopItemWidth();

          if (ImGui::Button("Freeze time"))  g_settings.time_scale = 0.0f;
          ImGui::SameLine();
          if (ImGui::Button("Normal speed")) g_settings.time_scale = 72.0f;
          ImGui::SameLine();
          if (ImGui::Button("Fast (10x)"))   g_settings.time_scale = 720.0f;
        }

        // Shadows
        if (ImGui::CollapsingHeader("Shadows", ImGuiTreeNodeFlags_DefaultOpen)) {
          ImGui::Checkbox("Enabled##shadows", &g_settings.shadows_enabled);
          ImGui::PushItemWidth(180);
          ImGui::SliderFloat("Distance", &g_settings.shadow_distance,
                             40.0f, 256.0f, "%.0f");
          ImGui::PopItemWidth();
        }

        // Clouds
        if (ImGui::CollapsingHeader("Clouds", ImGuiTreeNodeFlags_DefaultOpen)) {
          ImGui::Checkbox("Enabled", &g_settings.clouds_enabled);
          ImGui::PushItemWidth(180);
          ImGui::SliderFloat("Height",  &g_settings.cloud_height,
                             100.0f, 250.0f, "%.0f");
          ImGui::SliderFloat("Speed",   &g_settings.cloud_speed,
                             0.0f, 5.0f, "%.1f");
          ImGui::PopItemWidth();
        }

        ImGui::EndTabItem();
      }

      ImGui::EndTabBar();
    }

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
    // Debug panel toggle – release/recapture cursor so ImGui is usable
    case GLFW_KEY_F1:
      g_settings.show_debug = !g_settings.show_debug;
      if (g_settings.show_debug) {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
      } else {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        engine->first_mouse = true; // prevent camera jump on close
      }
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

  // Don't process mouse look when inventory or debug panel is open
  if (engine->world.getPlayer()->isInventoryOpen()) return;
  if (g_settings.show_debug) return;

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
    if (ray.hit && ray.block_type != BlockType::BEDROCK
                && ray.block_type != BlockType::WATER) {
      engine->world.setBlockAt(ray.block_pos, BlockType::AIR);
    }
  } else if (button == GLFW_MOUSE_BUTTON_RIGHT) {
    // Place block (right click)
    if (ray.hit) {
      glm::ivec3 place_pos = ray.block_pos + ray.normal;

      // Don't place outside world height bounds
      if (place_pos.y < 0 || place_pos.y >= kChunkHeight) return;

      // Don't place if the target cell is already occupied by a solid block
      BlockType existing = engine->world.getBlockAt(
          glm::vec3(place_pos) + glm::vec3(0.5f));
      if (existing != BlockType::AIR && !isLiquid(existing)) return;

      // Get the block type from the selected hotbar slot
      BlockType block_to_place = player->getHotbarBlock();
      if (block_to_place == BlockType::AIR) return;

      // Prevent placing the block inside the player's AABB
      float height = player->isCrouching() ? kPlayerCrouchHeight : kPlayerHeight;
      glm::vec3 ppos = player->getPosition();
      float pMinX = ppos.x - kPlayerHalfWidth;
      float pMaxX = ppos.x + kPlayerHalfWidth;
      float pMinY = ppos.y;
      float pMaxY = ppos.y + height;
      float pMinZ = ppos.z - kPlayerHalfWidth;
      float pMaxZ = ppos.z + kPlayerHalfWidth;

      float bMinX = static_cast<float>(place_pos.x);
      float bMaxX = bMinX + 1.0f;
      float bMinY = static_cast<float>(place_pos.y);
      float bMaxY = bMinY + 1.0f;
      float bMinZ = static_cast<float>(place_pos.z);
      float bMaxZ = bMinZ + 1.0f;

      bool overlaps = pMinX < bMaxX && pMaxX > bMinX &&
                      pMinY < bMaxY && pMaxY > bMinY &&
                      pMinZ < bMaxZ && pMaxZ > bMinZ;
      if (overlaps) return;

      engine->world.setBlockAt(place_pos, block_to_place);
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
