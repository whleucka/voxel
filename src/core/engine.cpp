#include "core/engine.hpp"
#include "block/block_data.hpp"
#include "core/config.hpp"
#include "core/constants.hpp"
#include "world/world_save.hpp"
#include "core/settings.hpp"
#include "render/renderer.hpp"
#include "imgui/backends/imgui_impl_glfw.h"
#include "imgui/backends/imgui_impl_opengl3.h"
#include "imgui/imgui.h"
#include "util/memory.hpp"
#include <algorithm>
#include <string>
#include <vector>
#include <GLFW/glfw3.h>
#include <glm/ext/scalar_constants.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <cstdio>
#include <cstdlib>
#include <iostream>

Engine::~Engine() { cleanup(); }

bool Engine::init() {
  // world_seed was already resolved from voxel.properties (blank = random).
  // Offset noise coordinates to get a unique world each seed.
  // Capped at 5000 units — large enough for variety, small enough to avoid
  // degenerate all-ocean regions that appear at extreme offsets.
  g_settings.noise_offset = glm::vec2(
      static_cast<float>(g_settings.world_seed % 5000),
      static_cast<float>((g_settings.world_seed / 5000) % 5000)
  );
  // Note: a loaded save overrides seed/noise_offset in run() before world gen.

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

  if (g_settings.fullscreen) {
    win_width = mode->width;
    win_height = mode->height;
  } else {
    win_width = g_settings.window_width;
    win_height = g_settings.window_height;
  }
  last_x = win_width / 2.0f;
  last_y = win_height / 2.0f;

  window = glfwCreateWindow(win_width, win_height, win_title.c_str(),
                            g_settings.fullscreen ? primaryMonitor : nullptr,
                            nullptr);
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

  if (!hud.init("res/block_atlas.png", "res/item_atlas.png"))
    std::cerr << "HUD icons unavailable; drawing empty slots\n";
  if (!entity_renderer.init())
    std::cerr << "Entity textures unavailable\n";

  return true;
}

void Engine::run() {
  // Resolve the active save directory and load it if it already exists.
  // An existing world keeps its own seed/clock; the properties seed only
  // matters when creating a fresh world.
  if (game.load(WorldSave::worldDir(g_settings.world_name)))
    std::cout << "Loaded world '" << g_settings.world_name << "' (seed "
              << g_settings.world_seed << ")\n";
  else
    std::cout << "New world '" << g_settings.world_name << "' (seed "
              << g_settings.world_seed << ")\n";

  game.world().init();

  // Join the in-process game; the Joined reply creates the local player.
  game.addClient(connection);
  connection.send(Join{});
  game.pump();
  handleEvents();
  if (!player) {
    std::cerr << "Failed to join game\n";
    return;
  }

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

  game.save(); // persist on quit
}

// ─── Input Processing (polled every frame) ─────────────────────────────

void Engine::processInput(GLFWwindow *window) {
  // Don't process movement when inventory is open or while dead
  if (player->isInventoryOpen() || vitals_view.dead) return;

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
  // Physics runs locally; the Game is told where we ended up, and streams
  // chunks around every player's reported position when it pumps.
  player->update(delta_time, &game.world());
  const Camera &cam = player->getCamera();
  MovePlayer move;
  move.position = player->getPosition();
  move.yaw = cam.yaw;
  move.pitch = cam.pitch;
  move.fly = player->isFlyMode();
  move.crouch = player->isCrouching();
  move.on_ground = player->isOnGround();
  move.sprinting = player->isSprinting();
  move.in_water = player->isInWater();
  move.eyes_underwater = player->isUnderwater();
  connection.send(move);

  game.pump();
  game.advance(delta_time);
  handleEvents();

  game.world().update(delta_time);

  if (g_settings.wireframe) {
    glLineWidth(2.0f);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
  } else {
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
  }
}

// ─── Events from the Game ──────────────────────────────────────────────

void Engine::handleEvents() {
  std::vector<Event> events;
  connection.poll(events);
  for (const Event &ev : events) {
    if (const auto *joined = std::get_if<Joined>(&ev)) {
      player_id = joined->id;
      player = std::make_unique<Player>(joined->position);
      player->getCamera().setPosition(joined->position);
      Camera &cam = player->getCamera();
      cam.yaw = joined->yaw;
      cam.pitch = joined->pitch;
      cam.processMouseMovement(0.0f, 0.0f); // refresh direction vectors
      player->setSelectedHotbarSlot(joined->slot);
      *player->getFlyModePtr() = joined->fly;
    }
    else if (const auto *rejected = std::get_if<CommandRejected>(&ev)) {
      last_rejection = rejected->reason;
    }
    else if (const auto *contents = std::get_if<InventoryContents>(&ev)) {
      inventory_view = contents->slots;
      inventory_screen.sync(inventory_view);
    }
    else if (const auto *vitals = std::get_if<VitalsChanged>(&ev)) {
      if (vitals->health < vitals_view.health) hud.flashDamage(glfwGetTime());
      vitals_view = *vitals;
      player->setSprintAllowed(vitals->can_sprint);
    }
    else if (const auto *respawned = std::get_if<Respawned>(&ev)) {
      player->respawnAt(respawned->position);
    }
    else if (const auto *snap = std::get_if<EntitySnapshot>(&ev)) {
      entity_interp.push(*snap);
      last_snapshot_time = glfwGetTime();
    }
    // BlockChanged needs no handling yet: in single-player the client renders
    // the Game's own World, which is already up to date.
  }
}

// ─── Render ────────────────────────────────────────────────────────────

void Engine::render() {
  glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  // Render scene
  glm::mat4 view = player->getCamera().getViewMatrix();
  glm::mat4 proj = glm::perspective(
      glm::radians(g_settings.fov),                  // field of view from settings
      (float)win_width / (float)win_height,           // aspect ratio
      0.01f,                                          // near plane (small = less wall clipping in tight spaces)
      1000.0f                                         // far plane
  );

  game.world().render(view, proj, game.clock().fractionOfDay(), win_width,
                      win_height, player->getPosition(), player->isUnderwater());

  // Entities are drawn between the last two snapshots (one tick behind).
  double now = glfwGetTime();
  float alpha = static_cast<float>((now - last_snapshot_time) / Game::kTickSeconds);
  entity_views = entity_interp.views(alpha);
  entity_renderer.draw(entity_views, game.world(), view, proj,
                       player->getCamera().position, player->isUnderwater(),
                       game.clock().fractionOfDay(), now);

  // ImGui frame must always be opened here so both renderCrosshair() and
  // debug() can safely submit widgets regardless of game state.
  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();

  renderCrosshair();

  const ImGuiIO &io = ImGui::GetIO();
  HudLayout layout(io.DisplaySize.x, io.DisplaySize.y);
  if (player->isInventoryOpen()) {
    hud.drawInventoryScreen(layout, inventory_view, inventory_screen,
                            {io.MousePos.x, io.MousePos.y});
  } else {
    hud.drawHotbar(layout, inventory_view, player->getSelectedHotbarSlot(),
                   glfwGetTime());
    hud.drawVitals(layout, vitals_view, glfwGetTime());
  }
  if (vitals_view.dead) hud.drawDeathScreen();

  debug();
}

// ─── Crosshair ─────────────────────────────────────────────────────────

void Engine::renderCrosshair() {
  // Don't show crosshair when inventory is open
  if (player->isInventoryOpen()) return;

  // Dark overlay when the camera eye is inside a solid block (e.g. clipping into a wall)
  {
    glm::vec3 eyePos = player->getEyePosition();
    BlockType eyeBlock = game.world().getBlockAt(eyePos);
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
          ImGui::Text("Chunks     : %zu",     game.world().getChunkCount());
          const RenderStats &rs = game.world().getRenderStats();
          ImGui::Text("Drawn      : %d (main)", rs.chunks_drawn);
          ImGui::Text("Shadow     : %d / %d",  rs.shadow_chunks_drawn,
                      rs.shadow_chunks_total);
          ImGui::Text("Game time  : %02d:%02d", game.clock().hour(), game.clock().minute());
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
          const ItemStack &held = inventory_view[player->getSelectedHotbarSlot()];
          std::string_view held_name = held.empty() ? "empty" : itemDef(held.id).name;
          ImGui::Text("Hotbar slot : %d [%.*s x%d]",
                      (player->getSelectedHotbarSlot() + 1) % kHotbarSlots,
                      static_cast<int>(held_name.size()), held_name.data(),
                      held.count);
          if (!last_rejection.empty())
            ImGui::Text("Rejected   : %s", last_rejection.c_str());
        }

        debugInventory();

        // World / Raycast
        if (ImGui::CollapsingHeader("World")) {
          const Camera&   cam = player->getCamera();
          RaycastResult   ray = game.world().raycast(cam.position, cam.front, kPlayerReach);
          if (ray.hit) {
            ImGui::Text("Block  (%d, %d, %d)",
                        ray.block_pos.x, ray.block_pos.y, ray.block_pos.z);
            ImGui::Text("Normal (%d, %d, %d)",
                        ray.normal.x, ray.normal.y, ray.normal.z);
            ImGui::Text("Dist   %.2f", ray.distance);
            // Light at the empty cell in front of the hit face (where a torch
            // would sit) — handy for verifying block-light propagation.
            glm::ivec3 lit = ray.block_pos + ray.normal;
            ImGui::Text("Sky %2u   Block %2u",
                        game.world().getSkyLightAt(lit), game.world().getBlockLightAt(lit));
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
          // Time of day in hours (0–24); changes go to the Game as a command
          float tod_hours = game.clock().time_of_day / 3600.0f;
          ImGui::PushItemWidth(180);
          if (ImGui::SliderFloat("Time of day", &tod_hours, 0.0f, 24.0f, "%.1f h"))
            connection.send(SetTimeOfDay{tod_hours * 3600.0f});

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

          static const char *kSizeLabels[] = {"1024", "2048", "4096"};
          static const int   kSizeVals[]   = {1024, 2048, 4096};
          int sizeIdx = g_settings.shadow_map_size == 1024 ? 0
                      : g_settings.shadow_map_size == 4096 ? 2 : 1;
          if (ImGui::Combo("Map size", &sizeIdx, kSizeLabels, 3))
            g_settings.shadow_map_size = kSizeVals[sizeIdx];
          ImGui::PopItemWidth();
        }

        // Clouds
        if (ImGui::CollapsingHeader("Clouds", ImGuiTreeNodeFlags_DefaultOpen)) {
          ImGui::Checkbox("Enabled", &g_settings.clouds_enabled);
          ImGui::PushItemWidth(180);
          ImGui::SliderFloat("Height",  &g_settings.cloud_height,
                             100.0f, 250.0f, "%.0f");
          ImGui::SliderInt("Thickness", &g_settings.cloud_thickness,
                           1, 16);
          ImGui::SliderFloat("Speed",   &g_settings.cloud_speed,
                             0.0f, 5.0f, "%.1f");
          ImGui::PopItemWidth();
        }

        // Persist to voxel.properties
        ImGui::Separator();
        static double saved_at = -10.0;
        if (ImGui::Button("Save settings"))
          if (saveServerProperties()) saved_at = ImGui::GetTime();
        if (ImGui::GetTime() - saved_at < 2.0) {
          ImGui::SameLine();
          ImGui::TextColored(ImVec4(0.4f, 1.f, 0.4f, 1.f),
                             "Saved to voxel.properties");
        }

        static double world_saved_at = -10.0;
        if (ImGui::Button("Save world (F5)")) {
          connection.send(Save{});
          world_saved_at = ImGui::GetTime();
        }
        if (ImGui::GetTime() - world_saved_at < 2.0) {
          ImGui::SameLine();
          ImGui::TextColored(ImVec4(0.4f, 1.f, 0.4f, 1.f), "World saved");
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

// ─── Debug: Inventory ──────────────────────────────────────────────────
// Until the hud module draws a hotbar and inventory screen, the inventory is
// shown here, along with a "give item" control for testing and creative play.

void Engine::debugInventory() {
  if (!ImGui::CollapsingHeader("Inventory", ImGuiTreeNodeFlags_DefaultOpen))
    return;

  auto slotText = [](const ItemStack &s) {
    if (s.empty()) return std::string("-");
    return std::string(itemDef(s.id).name) + " x" + std::to_string(s.count);
  };

  ImGui::TextDisabled("Hotbar");
  for (int i = 0; i < kHotbarSlots; ++i) {
    bool selected = i == player->getSelectedHotbarSlot();
    ImGui::TextColored(selected ? ImVec4(1.f, 0.9f, 0.3f, 1.f)
                                : ImVec4(0.85f, 0.85f, 0.85f, 1.f),
                       "%s%d  %s", selected ? ">" : " ", (i + 1) % kHotbarSlots,
                       slotText(inventory_view[i]).c_str());
  }

  ImGui::TextDisabled("Storage");
  bool any = false;
  for (int i = kHotbarSlots; i < kInventorySlots; ++i) {
    if (inventory_view[i].empty()) continue;
    ImGui::Text("  %2d  %s", i, slotText(inventory_view[i]).c_str());
    any = true;
  }
  if (!any) ImGui::TextDisabled("  (empty)");

  // Every item this build knows, for the give combo.
  static const std::vector<ItemId> all_items = [] {
    std::vector<ItemId> out;
    for (uint16_t v = 1; v <= static_cast<uint16_t>(kLastMaterialItem); ++v)
      if (isValidItem(static_cast<ItemId>(v))) out.push_back(static_cast<ItemId>(v));
    return out;
  }();
  static int give_index = 0;
  static int give_count = 16;

  ImGui::Separator();
  ImGui::PushItemWidth(150);
  std::string_view current = itemDef(all_items[give_index]).name;
  if (ImGui::BeginCombo("##give_item", std::string(current).c_str())) {
    for (int i = 0; i < static_cast<int>(all_items.size()); ++i) {
      std::string name(itemDef(all_items[i]).name);
      if (ImGui::Selectable(name.c_str(), i == give_index)) give_index = i;
    }
    ImGui::EndCombo();
  }
  ImGui::SameLine();
  ImGui::PushItemWidth(80);
  ImGui::InputInt("##give_count", &give_count);
  give_count = std::clamp(give_count, 1, 64 * kInventorySlots);
  ImGui::PopItemWidth();
  ImGui::PopItemWidth();
  ImGui::SameLine();
  if (ImGui::Button("Give"))
    connection.send(GiveItem{all_items[give_index],
                             static_cast<uint16_t>(give_count)});
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
      engine->player->toggleFlyMode();
      break;

    // Quick-save the world
    case GLFW_KEY_F5:
      engine->connection.send(Save{});
      break;

    // Inventory toggle
    case GLFW_KEY_E:
      engine->setInventoryOpen(!engine->player->isInventoryOpen());
      break;

    // Esc closes the inventory screen first; otherwise it quits
    case GLFW_KEY_ESCAPE:
      if (engine->player->isInventoryOpen())
        engine->setInventoryOpen(false);
      else
        glfwSetWindowShouldClose(window, true);
      break;

    // Hotbar slots: keys 1..9, then 0 for the tenth
    case GLFW_KEY_1: engine->selectSlot(0); break;
    case GLFW_KEY_2: engine->selectSlot(1); break;
    case GLFW_KEY_3: engine->selectSlot(2); break;
    case GLFW_KEY_4: engine->selectSlot(3); break;
    case GLFW_KEY_5: engine->selectSlot(4); break;
    case GLFW_KEY_6: engine->selectSlot(5); break;
    case GLFW_KEY_7: engine->selectSlot(6); break;
    case GLFW_KEY_8: engine->selectSlot(7); break;
    case GLFW_KEY_9: engine->selectSlot(8); break;
    case GLFW_KEY_0: engine->selectSlot(9); break;

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
  if (engine->player->isInventoryOpen()) return;
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

  engine->player->processMouseMovement(xoffset, yoffset);
}

// ─── Mouse Button Callback ─────────────────────────────────────────────

void Engine::mouseButtonCallback(GLFWwindow *window, int button, int action, int mods) {
  Engine *engine = reinterpret_cast<Engine *>(glfwGetWindowUserPointer(window));

  if (action != GLFW_PRESS) return;

  // Inventory screen: clicks move items instead of breaking/placing. Clicks
  // on an ImGui window (the F1 panel) belong to that window.
  if (engine->player->isInventoryOpen()) {
    const ImGuiIO &io = ImGui::GetIO();
    if (io.WantCaptureMouse) return;
    HudLayout layout(io.DisplaySize.x, io.DisplaySize.y);
    std::optional<int> slot = layout.hitTestScreen(io.MousePos.x, io.MousePos.y);
    const Inventory::Slots &inv = engine->inventory_view;
    InventoryScreen &screen = engine->inventory_screen;

    std::optional<MoveItems> move;
    if (button == GLFW_MOUSE_BUTTON_LEFT)
      move = (mods & GLFW_MOD_SHIFT) ? screen.shiftClick(slot, inv)
                                     : screen.leftClick(slot, inv);
    else if (button == GLFW_MOUSE_BUTTON_RIGHT)
      move = screen.rightClick(slot, inv);
    if (move) engine->connection.send(*move);
    return;
  }

  if (engine->vitals_view.dead) return;

  // The client only aims; the Game validates and applies the edit on the next
  // pump, which runs before the next frame renders.
  auto& player = engine->player;
  const Camera& cam = player->getCamera();
  RaycastResult ray = engine->game.world().raycast(cam.position, cam.front, kPlayerReach);
  if (!ray.hit) return;

  if (button == GLFW_MOUSE_BUTTON_LEFT) {
    engine->connection.send(BreakBlock{ray.block_pos});
  } else if (button == GLFW_MOUSE_BUTTON_RIGHT) {
    engine->connection.send(PlaceBlock{ray.block_pos + ray.normal});
  }
}

// ─── Scroll Callback (hotbar slot cycling) ─────────────────────────────

void Engine::scrollCallback(GLFWwindow *window, double, double yoffset) {
  Engine *engine = reinterpret_cast<Engine *>(glfwGetWindowUserPointer(window));
  auto& player = engine->player;

  int slot = player->getSelectedHotbarSlot();
  if (yoffset < 0) {
    slot = (slot + 1) % kHotbarSlots;
  } else if (yoffset > 0) {
    slot = (slot - 1 + kHotbarSlots) % kHotbarSlots;
  }
  engine->selectSlot(slot);
}

void Engine::setInventoryOpen(bool open) {
  if (player->isInventoryOpen() == open) return;
  player->toggleInventory();
  if (open) {
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
  } else {
    inventory_screen.close(); // drop any pick-up; nothing was moved yet
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    first_mouse = true; // prevent mouse jump when closing inventory
  }
}

// The local Player mirrors the selection immediately (it picks the block to
// place); the Game holds the authoritative copy.
void Engine::selectSlot(int slot) {
  player->setSelectedHotbarSlot(slot);
  connection.send(SelectSlot{slot});
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
  hud.shutdown(); // GL textures; must go before the context does
  entity_renderer.shutdown();
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();

  if (window) {
    glfwDestroyWindow(window);
    glfwTerminate();
    window = nullptr;
  }
}
