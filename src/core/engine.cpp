#include "core/engine.hpp"
#include "ui/cursor.hpp"
#include "ui/menu_style.hpp"
#include "block/block_data.hpp"
#include "core/config.hpp"
#include "core/constants.hpp"
#include "core/seed.hpp"
#include "world/world_save.hpp"
#include "core/settings.hpp"
#include "render/renderer.hpp"
#include "imgui/backends/imgui_impl_glfw.h"
#include "imgui/backends/imgui_impl_opengl3.h"
#include "imgui/imgui.h"
#include "entity/player_model.hpp"
#include "physics/ray.hpp"
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
  g_settings.noise_offset = noiseOffsetFor(g_settings.world_seed);
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
  shown_vsync = g_settings.vsync;
  shown_fullscreen = g_settings.fullscreen;

  // ImGui
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO &io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

  ImGui::StyleColorsDark();
  applyMenuStyle();

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
  menu.open();
  while (!glfwWindowShouldClose(window)) {
    float currentFrame = glfwGetTime();
    delta_time = currentFrame - last_frame;
    last_frame = currentFrame;
    applyDisplaySettings();
    updateCursor();

    if (playing()) {
      processInput(window);
      update();
      if (playing()) render(); // the update may have ended the game
    } else {
      menuFrame();
    }

    glfwSwapBuffers(window);
    glfwPollEvents();
  }

  endSession(); // persist on quit
  scene.close(); // while the GL context is still alive
}

// ─── Display settings ──────────────────────────────────────────────────
// F3, F11 and the settings page only change g_settings; the window follows
// here, once per frame.

void Engine::applyDisplaySettings() {
  if (g_settings.vsync != shown_vsync) {
    shown_vsync = g_settings.vsync;
    glfwSwapInterval(shown_vsync ? 1 : 0);
  }
  if (g_settings.fullscreen == shown_fullscreen) return;
  shown_fullscreen = g_settings.fullscreen;
  if (shown_fullscreen) {
    glfwGetWindowPos(window, &win_pos_x, &win_pos_y);
    glfwGetWindowSize(window, &win_width, &win_height);
    GLFWmonitor *monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode *mode = glfwGetVideoMode(monitor);
    glfwSetWindowMonitor(window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
  } else {
    glfwSetWindowMonitor(window, nullptr, win_pos_x, win_pos_y, win_width, win_height, 0);
  }
}

// ─── Title menu ────────────────────────────────────────────────────────

void Engine::menuFrame() {
  // A guest waits here for the host's Joined (or a refusal).
  if (session) {
    session->frame(delta_time, glfwGetTime());
    handleEvents();
    if (session && !player && glfwGetTime() - join_started > 10.0)
      endSession("The host didn't answer");
  }

  // The last played world, circling behind the menu (not while joining:
  // the scene and a guest's world can't share the global seed).
  if (!session && !scene.isOpen()) scene.open(WorldSave::worldDir(g_settings.world_name));

  glClearColor(0.10f, 0.12f, 0.16f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  scene.frame(delta_time, win_width, win_height);
  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();
  // The scene fades in from the menu colour as its chunks arrive.
  float cover = scene.isOpen() ? scene.cover() : 1.0f;
  if (cover > 0.0f)
    ImGui::GetBackgroundDrawList()->AddRectFilled(
        ImVec2(0, 0), ImVec2(static_cast<float>(win_width), static_cast<float>(win_height)),
        IM_COL32(26, 31, 41, static_cast<int>(cover * 255)));
  std::string name = g_settings.player_name;
  std::string last_world = g_settings.world_name;
  const ImGuiIO &io = ImGui::GetIO();
  hud.drawLogo(io.DisplaySize.x, io.DisplaySize.y);
  TitleMenu::Choice choice = menu.draw(glfwGetTime(), name, last_world,
                                       static_cast<uint16_t>(g_settings.server_port));
  g_settings.player_name = name;
  if (last_world != g_settings.world_name) { // a rename, or a world chosen to play
    g_settings.world_name = last_world;
    saveServerProperties();
  }
  ImGui::Render();
  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

  if (session) return; // already joining: ignore the buttons
  if (choice.kind == TitleMenu::Choice::Quit) glfwSetWindowShouldClose(window, true);
  else if (choice.kind != TitleMenu::Choice::None) startFromMenu(choice);
}

void Engine::startFromMenu(const TitleMenu::Choice &choice) {
  saveServerProperties(); // remember the name (and the last world) for next time
  // World generation reads the global seed: the scene must be gone first.
  scene.close();

  Session::Options opts;
  opts.name = g_settings.player_name;
  opts.save_dir = WorldSave::worldDir(choice.world);
  opts.world_name = choice.world;
  opts.port = static_cast<uint16_t>(g_settings.server_port);

  std::string error;
  std::unique_ptr<Session> s;
  switch (choice.kind) {
    case TitleMenu::Choice::Play:
      s = Session::local(Session::Mode::Single, opts, error);
      break;
    case TitleMenu::Choice::Host:
      s = Session::local(Session::Mode::Host, opts, error);
      break;
    case TitleMenu::Choice::Join:
      opts.port = choice.address.port;
      menu.setStatus("Joining " + choice.address.host + "...");
      s = Session::join(choice.address.host, opts, error);
      break;
    default:
      return;
  }
  if (!s) {
    menu.open(error);
    return;
  }
  join_started = glfwGetTime();
  startSession(std::move(s));
}

void Engine::updateCursor() {
  CursorState state;
  state.playing = playing();
  state.paused = paused();
  state.debug = g_settings.show_debug;
  state.inventory = player && player->isInventoryOpen();
  bool free = cursorFree(state) || g_settings.show_cursor;

  // While captured, ImGui must not set its own cursor (it would show it).
  ImGuiIO &io = ImGui::GetIO();
  if (free) io.ConfigFlags &= ~ImGuiConfigFlags_NoMouseCursorChange;
  else io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;

  int want = free ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED;
  if (glfwGetInputMode(window, GLFW_CURSOR) != want) {
    glfwSetInputMode(window, GLFW_CURSOR, want);
    first_mouse = true; // no camera jump when it's captured again
  }
  // On Wayland (seen on Hyprland), capturing while the pointer is over the
  // window locks the pointer and hides the cursor in one go, and the
  // compositor shows the cursor again once the lock takes effect. Asking for
  // no cursor again (this keeps the lock) makes it stick; it's one tiny
  // request a frame, and also covers anything else that brings it back.
  if (!free) glfwSetCursor(window, nullptr);
}

void Engine::startSession(std::unique_ptr<Session> s) {
  session = std::move(s);
  if (!session) return;
  // Fresh client-side view of the new game.
  player.reset();
  player_id = 0;
  respawn_count = 0;
  last_rejection.clear();
  inventory_view = {};
  vitals_view = {Vitals::kMax, Vitals::kMax, Vitals::kMax, true, false};
  entity_interp = EntityInterpolator{};
  entity_views.clear();
  player_views.clear();
  world().init();
  // The Joined reply creates the local player (at once in-process; a guest
  // waits for the host in the menu).
  handleEvents();
}

void Engine::endSession(std::string message) {
  if (!session) return;
  session->end();
  session.reset();
  player.reset();
  pause.close();
  g_settings.show_debug = false;
  menu.open(std::move(message));
}

// ─── Input Processing (polled every frame) ─────────────────────────────

void Engine::processInput(GLFWwindow *window) {
  // Don't process movement when inventory is open or while dead
  if (player->isInventoryOpen() || vitals_view.dead || paused()) return;

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
  if (!PauseMenu::worldRuns(session->mode() == Session::Mode::Single, paused())) return;
  // Physics runs locally; the Game is told where we ended up, and streams
  // chunks around every player's reported position when it pumps.
  player->update(delta_time, &world());
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
  move.respawn = respawn_count;
  connection().send(move);
  session->follow(player->getPosition());

  session->frame(delta_time, glfwGetTime());
  handleEvents();
  if (!session) return; // the game ended (host left, connection lost)

  world().update(delta_time);

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
  session->poll(events);
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
      game_mode = joined->mode;
      *player->getFlyModePtr() = joined->fly;
      player->setFlyAllowed(rulesFor(game_mode).fly);
      menu.close();
    }
    else if (const auto *refused = std::get_if<JoinRefused>(&ev)) {
      return endSession(refused->reason);
    }
    else if (const auto *gone = std::get_if<Disconnected>(&ev)) {
      return endSession(gone->reason);
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
      if (player) player->setSprintAllowed(vitals->can_sprint);
    }
    else if (const auto *respawned = std::get_if<Respawned>(&ev); respawned && player) {
      player->respawnAt(respawned->position);
      respawn_count = respawned->respawn;
    }
    else if (const auto *knocked = std::get_if<KnockedBack>(&ev); knocked && player) {
      player->knock(knocked->velocity);
    }
    else if (const auto *snap = std::get_if<EntitySnapshot>(&ev)) {
      entity_interp.push(*snap);
      last_snapshot_time = glfwGetTime();
    }
    // BlockChanged, Welcome and TimeSync are world-level: the Session has
    // already applied them (a guest's world), or the world is the Game's own.
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

  world().render(view, proj, session->clock().fractionOfDay(), win_width,
                 win_height, player->getPosition(), player->isUnderwater());

  // Entities are drawn between the last two snapshots (one tick behind).
  double now = glfwGetTime();
  float alpha = static_cast<float>((now - last_snapshot_time) / Game::kTickSeconds);
  entity_views = entity_interp.views(alpha);
  player_views = entity_interp.players(alpha);
  entity_renderer.draw(entity_views, player_views, world(), view, proj,
                       player->getCamera().position, player->isUnderwater(),
                       session->clock().fractionOfDay(), now);

  // ImGui frame must always be opened here so both renderCrosshair() and
  // debug() can safely submit widgets regardless of game state.
  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();

  renderCrosshair();
  renderNameTags(proj * view);

  const ImGuiIO &io = ImGui::GetIO();
  HudLayout layout(io.DisplaySize.x, io.DisplaySize.y);
  if (player->isInventoryOpen()) {
    hud.drawInventoryScreen(layout, inventory_view, inventory_screen,
                            {io.MousePos.x, io.MousePos.y});
    if (rulesFor(game_mode).palette)
      hud.drawPalette(layout, paletteItems(), {io.MousePos.x, io.MousePos.y});
  } else {
    hud.drawHotbar(layout, inventory_view, player->getSelectedHotbarSlot(),
                   glfwGetTime());
    hud.drawVitals(layout, vitals_view, glfwGetTime());
  }
  if (vitals_view.dead) hud.drawDeathScreen();
  PauseMenu::Action action = pause.draw(session->mode() == Session::Mode::Single,
                                        session->isGuest(), glfwGetTime());

  debug();
  pauseAction(action); // after the frame: it may end the game
}

void Engine::pauseAction(PauseMenu::Action action) {
  switch (action) {
    case PauseMenu::Action::None: break;
    case PauseMenu::Action::Save:
      connection().send(Save{});
      session->frame(0.0f, glfwGetTime()); // a paused game still takes the save
      pause.saved(glfwGetTime());
      break;
    case PauseMenu::Action::ReturnToTitle: endSession(); break;
    case PauseMenu::Action::Exit:
      endSession();
      glfwSetWindowShouldClose(window, true);
      break;
  }
}

// ─── Name tags ─────────────────────────────────────────────────────────
// Each other player's name floats above their head, readable from any side.

void Engine::renderNameTags(const glm::mat4 &view_projection) {
  constexpr float kTagRange = 48.0f;
  ImDrawList *draw = ImGui::GetBackgroundDrawList();
  for (const PlayerView &p : player_views) {
    glm::vec3 above = p.position + glm::vec3(0.0f, kPlayerHeight + 0.35f, 0.0f);
    if (glm::distance(above, player->getCamera().position) > kTagRange) continue;
    auto at = projectToScreen(above, view_projection, static_cast<float>(win_width),
                              static_cast<float>(win_height));
    if (!at) continue;
    ImVec2 size = ImGui::CalcTextSize(p.name.c_str());
    ImVec2 lo(at->x - size.x * 0.5f - 4.0f, at->y - size.y - 2.0f);
    ImVec2 hi(at->x + size.x * 0.5f + 4.0f, at->y + 2.0f);
    draw->AddRectFilled(lo, hi, IM_COL32(0, 0, 0, 120), 3.0f);
    const glm::vec3 &c = playerColor(p.name);
    draw->AddText(ImVec2(lo.x + 4.0f, lo.y + 2.0f),
                  IM_COL32(255, 255, 255, 255), p.name.c_str());
    // A small stripe in their colour, to match the shirt.
    draw->AddRectFilled(ImVec2(lo.x, hi.y - 2.0f), ImVec2(hi.x, hi.y),
                        IM_COL32(c.r * 255, c.g * 255, c.b * 255, 255));
  }
}

// ─── Crosshair ─────────────────────────────────────────────────────────

void Engine::renderCrosshair() {
  // Dark overlay when the camera eye is inside a solid block (e.g. clipping
  // into a wall). Behind the HUD and menus, so the pause menu stays readable.
  {
    glm::vec3 eyePos = player->getEyePosition();
    BlockType eyeBlock = world().getBlockAt(eyePos);
    if (eyeBlock != BlockType::AIR && !isLiquid(eyeBlock)) {
      ImDrawList* overlay = ImGui::GetBackgroundDrawList();
      overlay->AddRectFilled(ImVec2(0, 0), ImVec2((float)win_width, (float)win_height),
                             IM_COL32(0, 0, 0, 200));
    }
  }

  // No crosshair over the inventory screen or the pause menu.
  if (player->isInventoryOpen() || paused()) return;

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
    ImGui::Begin("Grumblegrove###Voxel Engine", nullptr,
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
          ImGui::Text("Chunks     : %zu",     world().getChunkCount());
          const RenderStats &rs = world().getRenderStats();
          ImGui::Text("Drawn      : %d (main)", rs.chunks_drawn);
          ImGui::Text("Shadow     : %d / %d",  rs.shadow_chunks_drawn,
                      rs.shadow_chunks_total);
          ImGui::Text("Game time  : %02d:%02d", session->clock().hour(), session->clock().minute());
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

        debugMultiplayer();
        debugInventory();

        // Cheats are the host's: a guest's Game would refuse them anyway.
        bool cheats = !session->isGuest();
        if (ImGui::CollapsingHeader("Entities")) {
          ImGui::Text("Visible    : %zu", entity_views.size());
        }
        if (cheats && ImGui::CollapsingHeader("Spawn and difficulty")) {
          if (ImGui::Button("Spawn test creature"))
            connection().send(SpawnDebugEntity{EntityKind::TestCreature});
          // Surface enemies vanish in daylight: set the time to night first.
          for (EntityKind k : kEnemyKinds) {
            ImGui::SameLine();
            std::string label = std::string(entityType(k).name);
            if (ImGui::Button(label.c_str())) connection().send(SpawnDebugEntity{k});
          }
          // Read by the Game every tick; "Save settings" writes it out.
          int difficulty = static_cast<int>(g_settings.difficulty);
          const char *kDifficulties[] = {"Peaceful", "Easy", "Normal"};
          ImGui::PushItemWidth(180);
          if (ImGui::Combo("Difficulty", &difficulty, kDifficulties, 3))
            g_settings.difficulty = static_cast<Difficulty>(difficulty);
          ImGui::PopItemWidth();
        }

        // World / Raycast
        if (ImGui::CollapsingHeader("World")) {
          const Camera&   cam = player->getCamera();
          RaycastResult   ray = world().raycast(cam.position, cam.front, kPlayerReach);
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
                        world().getSkyLightAt(lit), world().getBlockLightAt(lit));
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
          ImGui::BeginDisabled(!player->isFlyAllowed());
          ImGui::Checkbox("Fly mode (creative)", player->getFlyModePtr());
          ImGui::EndDisabled();
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
          // Time of day in hours (0–24); changes go to the Game as a command.
          // The clock is the host's, so guests only see it.
          bool host_clock = !session->isGuest();
          float tod_hours = session->clock().time_of_day / 3600.0f;
          ImGui::PushItemWidth(180);
          ImGui::BeginDisabled(!host_clock);
          if (ImGui::SliderFloat("Time of day", &tod_hours, 0.0f, 24.0f, "%.1f h"))
            connection().send(SetTimeOfDay{tod_hours * 3600.0f});

          ImGui::SliderFloat("Time scale",       &g_settings.time_scale,
                             0.0f, 720.0f, "%.0f");
          ImGui::EndDisabled();
          ImGui::SliderFloat("Water fog",        &g_settings.water_fog_density,
                             0.0f, 0.20f,  "%.3f");
          ImGui::PopItemWidth();

          ImGui::BeginDisabled(!host_clock);
          if (ImGui::Button("Freeze time"))  g_settings.time_scale = 0.0f;
          ImGui::SameLine();
          if (ImGui::Button("Normal speed")) g_settings.time_scale = 72.0f;
          ImGui::SameLine();
          if (ImGui::Button("Fast (10x)"))   g_settings.time_scale = 720.0f;
          ImGui::EndDisabled();
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
        if (!session->isGuest() && ImGui::Button("Save world (F5)")) {
          connection().send(Save{});
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

// ─── Debug: Multiplayer ────────────────────────────────────────────────
// Who is playing, and for a host the address guests can type in.

void Engine::debugMultiplayer() {
  if (!ImGui::CollapsingHeader("Multiplayer", ImGuiTreeNodeFlags_DefaultOpen)) return;
  ImGui::Text("Name       : %s", g_settings.player_name.c_str());
  switch (session->mode()) {
    case Session::Mode::Single:
      ImGui::TextDisabled("Single player (Esc for the menu to host)");
      break;
    case Session::Mode::Host: {
      ImGui::Text("Hosting    : %zu player(s)", session->game()->playerCount());
      static const std::vector<std::string> addresses = net::lanAddresses();
      for (const std::string &a : addresses)
        ImGui::Text("Address    : %s:%u", a.c_str(), session->server()->port());
      if (addresses.empty()) ImGui::TextDisabled("No network address found");
      break;
    }
    case Session::Mode::Guest:
      ImGui::Text("Guest      : %zu other player(s) nearby", player_views.size());
      break;
  }
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
  ImGui::BeginDisabled(session->isGuest()); // a cheat: the host's
  bool give = ImGui::Button("Give");
  ImGui::EndDisabled();
  if (give)
    connection().send(GiveItem{all_items[give_index],
                             static_cast<uint16_t>(give_count)});
}

// ─── Key Callback (event-based, for toggle actions) ────────────────────

void Engine::keyCallback(GLFWwindow *window, int key, int, int action, int mods) {
  Engine *engine =
      reinterpret_cast<Engine *>(glfwGetWindowUserPointer(window));

  if (action != GLFW_PRESS) return;
  // In the menu, keys belong to ImGui (typing a name or an address).
  if (!engine->playing()) return;
  // Paused, only Esc counts; ImGui has the rest for moving around the menu.
  if (engine->paused() && key != GLFW_KEY_ESCAPE) return;

  switch (key) {
    // Debug panel toggle (the cursor follows in updateCursor)
    case GLFW_KEY_F1:
      g_settings.show_debug = !g_settings.show_debug;
      break;
    case GLFW_KEY_F2:
      g_settings.wireframe = !g_settings.wireframe;
      break;
    case GLFW_KEY_F3:
      g_settings.vsync = !g_settings.vsync; // applied next frame
      break;

    // Fly mode toggle
    case GLFW_KEY_T:
      engine->player->toggleFlyMode();
      break;

    // Quick-save the world
    case GLFW_KEY_F5:
      engine->connection().send(Save{});
      break;

    // Inventory toggle
    case GLFW_KEY_E:
      engine->setInventoryOpen(!engine->player->isInventoryOpen());
      break;

    // Q drops one of the held item, Ctrl+Q the whole stack
    case GLFW_KEY_Q:
      if (!engine->player->isInventoryOpen() && !engine->vitals_view.dead)
        engine->connection().send(DropItem{(mods & GLFW_MOD_CONTROL) != 0});
      break;

    // Esc closes the inventory screen first; otherwise it opens the pause
    // menu, backs out of its settings, or resumes. It never leaves the game.
    case GLFW_KEY_ESCAPE:
      if (engine->player->isInventoryOpen()) {
        engine->setInventoryOpen(false);
      } else {
        engine->pause.escape();
      }
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
    case GLFW_KEY_F11:
      g_settings.fullscreen = !g_settings.fullscreen; // applied next frame
      break;

    default:
      break;
  }
}

// ─── Mouse Move Callback ───────────────────────────────────────────────

void Engine::mouseCallback(GLFWwindow *window, double xpos, double ypos) {
  Engine *engine = reinterpret_cast<Engine *>(glfwGetWindowUserPointer(window));

  // Don't process mouse look in the menu, or when inventory or debug panel is open
  if (!engine->playing() || engine->player->isInventoryOpen() || engine->paused()) return;
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

  if (action != GLFW_PRESS || !engine->playing() || engine->paused()) return;

  // Inventory screen: clicks move items instead of breaking/placing. Clicks
  // on an ImGui window (the F1 panel) belong to that window.
  if (engine->player->isInventoryOpen()) {
    const ImGuiIO &io = ImGui::GetIO();
    if (io.WantCaptureMouse) return;
    HudLayout layout(io.DisplaySize.x, io.DisplaySize.y);
    std::optional<int> slot = layout.hitTestScreen(io.MousePos.x, io.MousePos.y);
    const Inventory::Slots &inv = engine->inventory_view;
    InventoryScreen &screen = engine->inventory_screen;

    // Creative palette: a click gives a full stack of that item.
    if (rulesFor(engine->game_mode).palette && !slot) {
      const std::vector<ItemId> &items = paletteItems();
      int count = static_cast<int>(items.size());
      if (auto pick = layout.hitTestPalette(io.MousePos.x, io.MousePos.y, count)) {
        screen.close(); // drop any pick-up; nothing was moved yet
        ItemId id = items[*pick];
        engine->connection().send(GiveItem{id, itemDef(id).max_stack});
        return;
      }
    }

    std::optional<MoveItems> move;
    if (button == GLFW_MOUSE_BUTTON_LEFT)
      move = (mods & GLFW_MOD_SHIFT) ? screen.shiftClick(slot, inv)
                                     : screen.leftClick(slot, inv);
    else if (button == GLFW_MOUSE_BUTTON_RIGHT)
      move = screen.rightClick(slot, inv);
    if (move) engine->connection().send(*move);
    return;
  }

  if (engine->vitals_view.dead) return;

  // The client only aims; the Game validates and applies the edit on the next
  // pump, which runs before the next frame renders.
  auto& player = engine->player;
  const Camera& cam = player->getCamera();
  RaycastResult ray = engine->world().raycast(cam.position, cam.front, kPlayerReach);

  // The nearest entity along the aim that `wanted` accepts, if it's nearer
  // than the block the ray hit.
  auto aimedEntity = [&](bool (*wanted)(EntityKind)) -> std::optional<EntityId> {
    std::optional<EntityId> target;
    float nearest = ray.hit ? ray.distance : kPlayerReach;
    for (const EntityView &v : engine->entity_views) {
      if (!wanted(v.kind)) continue;
      const EntityTypeDef &def = entityType(v.kind);
      glm::vec3 lo = v.position - glm::vec3(def.half_width, 0.0f, def.half_width);
      glm::vec3 hi = v.position + glm::vec3(def.half_width, def.height, def.half_width);
      auto t = rayAabb(cam.position, cam.front, lo, hi);
      if (t && *t <= nearest) {
        nearest = *t;
        target = v.id;
      }
    }
    return target;
  };

  if (button == GLFW_MOUSE_BUTTON_LEFT) {
    auto hittable = [](EntityKind k) { return entityType(k).max_health > 0; };
    if (auto target = aimedEntity(hittable))
      engine->connection().send(AttackEntity{*target});
    else if (ray.hit)
      engine->connection().send(BreakBlock{ray.block_pos});
  } else if (button == GLFW_MOUSE_BUTTON_RIGHT) {
    // Feed an animal, else eat held food, else place a block.
    const ItemStack &held = engine->inventory_view[player->getSelectedHotbarSlot()];
    if (auto animal = aimedEntity(isAnimal))
      engine->connection().send(InteractEntity{*animal});
    else if (!held.empty() && itemDef(held.id).food_heal > 0)
      engine->connection().send(EatItem{});
    else if (ray.hit)
      engine->connection().send(PlaceBlock{ray.block_pos + ray.normal});
  }
}

// ─── Scroll Callback (hotbar slot cycling) ─────────────────────────────

void Engine::scrollCallback(GLFWwindow *window, double, double yoffset) {
  Engine *engine = reinterpret_cast<Engine *>(glfwGetWindowUserPointer(window));
  if (!engine->playing() || engine->paused()) return;
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
  if (!open) inventory_screen.close(); // drop any pick-up; nothing was moved yet
}

// The local Player mirrors the selection immediately (it picks the block to
// place); the Game holds the authoritative copy.
void Engine::selectSlot(int slot) {
  player->setSelectedHotbarSlot(slot);
  connection().send(SelectSlot{slot});
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
