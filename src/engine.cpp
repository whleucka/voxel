#include "engine.hpp"
#include "block_data_manager.hpp"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "player.hpp"
#include "texture_manager.hpp"
#include "utils.hpp"
#include "chunk.hpp"
#include <GLFW/glfw3.h>
#include <glm/ext/scalar_constants.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <sys/resource.h>

size_t getMemoryUsage() {
  struct rusage usage;
  getrusage(RUSAGE_SELF, &usage);
  // Defaults to KB (long)
  return usage.ru_maxrss / 1024.0f; // MB
}

void keyCallback(GLFWwindow *window, int key, int, int action, int) {
  Engine *engine = reinterpret_cast<Engine *>(glfwGetWindowUserPointer(window));
  if (!engine) {
    return;
  }

  if (action == GLFW_PRESS) {
    engine->handleKeyPress(key, action);
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

  // Initial "safe" spawn
  player = new Player(&world, glm::vec3(0, 200, 0));

  renderer.init();
  loadAtlas("res/block_atlas.png");

  world.processUploads();

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
  // Spawn user (wait for chunk to generate)
  if (!spawn) {
    for (int y = Chunk::H - 1; y >= 0; --y) {
        if (world.getBlock(0, y, 0) != BlockType::AIR) {
            player->setPosition(glm::vec3(0, y + 2, 0));
            spawn = true;
            break;
        }
    }
  }

  game_clock.update(delta_time);
  player->update(delta_time);
  cloud_manager.update(player->getPosition());
  world.update(player->getCamera());
  world.processUploads();

  // Raycast for highlighted block
  auto &cam = player->getCamera();
  auto result = world.raycast(cam.getPosition(), cam.getFront(), 5.0f);
  if (result) {
    BlockType block_type = world.getBlock(std::get<0>(*result).x, std::get<0>(*result).y, std::get<0>(*result).z);
    if (BlockDataManager::getInstance().isSelectable(block_type)) {
      has_highlighted_block = true;
      highlighted_block_pos = std::get<0>(*result);
    } else {
      has_highlighted_block = false;
    }
  } else {
    has_highlighted_block = false;
  }

  if (wireframe) {
    glLineWidth(2.0f);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
  } else {
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
  }
}

void Engine::render() {
  // Set sky color based on time of day
  float time_fraction = game_clock.fractionOfDay();
  float t = time_fraction; // 0..1
  float ang = t * 2.0f * glm::pi<float>() -
              glm::half_pi<float>(); // rises ~0.25, sets ~0.75
  glm::vec3 sunDir =
      glm::normalize(glm::vec3(std::cos(ang), std::sin(ang), 0.25f));

  float h = sunDir.y;

  // Two smoothsteps: twilight below/above horizon
  float duskDawnBelow =
      glm::smoothstep(-0.15f, 0.00f, h); // -0.15→0: night → sunset
  float dawnNoonAbove =
      glm::smoothstep(0.00f, 0.25f, h); // 0→0.25: sunset → day

  glm::vec3 sky_color;
  if (h <= 0.0f) {
    // Below horizon: mostly night, a touch of sunset near horizon
    sky_color = glm::mix(glm::vec3(0.0f / 255.0f, 0.0f / 255.0f, 3.0f / 255.0f), glm::vec3(1.0f, 0.5f, 0.2f), duskDawnBelow);
  } else {
    // Above horizon: start at sunset near horizon, fade to day as sun climbs
    sky_color = glm::mix(glm::vec3(1.0f, 0.5f, 0.2f), glm::vec3(0.4f, 0.7f, 1.0f), dawnNoonAbove);
  }

  float horizonGlow = 1.0f - glm::smoothstep(0.05f, 0.25f, std::abs(h));
  sky_color = glm::mix(sky_color, glm::vec3(1.0f, 0.5f, 0.2f), 0.25f * horizonGlow);

  glClearColor(sky_color.r, sky_color.g, sky_color.b, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  // Gather chunk pointers from world
  std::vector<Chunk *> visible_chunks =
      world.getVisibleChunks(player->getCamera(), width, height);

  // Render clouds
  renderer.drawClouds(cloud_manager, player->getCamera(), width, height, time_fraction);

  // Render scene
  renderer.draw(visible_chunks, player->getCamera(), width, height,
                time_fraction);

  if (has_highlighted_block) {
    renderer.drawHighlight(player->getCamera(), highlighted_block_pos);
  }

  imgui();
}

void Engine::drawCrosshairImGui() {
  ImVec2 center = ImGui::GetMainViewport()->GetCenter();
  ImDrawList *dl = ImGui::GetForegroundDrawList();

  float size = 15.0f;
  float thickness = 4.0f;
  ImU32 color = IM_COL32(255, 255, 255, 128);

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
    ImGui::Text("Selected block: %s", BlockDataManager::getInstance().getName(selected_block_type).c_str());
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

void Engine::handleMouseClick(int button, int action, int) {
  if (action == GLFW_PRESS) {
    auto &cam = player->getCamera();
    auto result = world.raycast(cam.getPosition(), cam.getFront(), 5.0f);
    if (result) {
      BlockType block_type = world.getBlock(std::get<0>(*result).x, std::get<0>(*result).y, std::get<0>(*result).z);
      if (!BlockDataManager::getInstance().isSelectable(block_type)) {
        return;
      }
      auto [block_pos, normal] = *result;
      if (button == GLFW_MOUSE_BUTTON_RIGHT) {
        glm::ivec3 new_pos = block_pos + normal;
        world.setBlock(new_pos.x, new_pos.y, new_pos.z, selected_block_type);
      } else if (button == GLFW_MOUSE_BUTTON_LEFT) {
        world.setBlock(block_pos.x, block_pos.y, block_pos.z, BlockType::AIR);
      }

      int cx = floorDiv(block_pos.x, Chunk::W);
      int cz = floorDiv(block_pos.z, Chunk::L);
      world.remeshChunk(cx, cz);

      // Also update neighbors if the block is on a chunk boundary
      if (block_pos.x % Chunk::W == 0) {
        world.remeshChunk(cx - 1, cz);
      }
      if (block_pos.x % Chunk::W == Chunk::W - 1) {
        world.remeshChunk(cx + 1, cz);
      }
      if (block_pos.z % Chunk::L == 0) {
        world.remeshChunk(cx, cz - 1);
      }
      if (block_pos.z % Chunk::L == Chunk::L - 1) {
        world.remeshChunk(cx, cz + 1);
      }
    }
  }
}

void Engine::changeSelectedBlockType(BlockType type) {
  selected_block_type = type;
}

void Engine::handleKeyPress(int key, int action) {
  if (action == GLFW_PRESS) {
    if (key == GLFW_KEY_F3) {
      debug = !debug;
    }
    if (key == GLFW_KEY_F4) {
      wireframe = !wireframe;
    }

    if (key >= GLFW_KEY_0 && key <= GLFW_KEY_9) {
      int block_num = key - GLFW_KEY_0;
      switch (block_num) {
      case 1:
        changeSelectedBlockType(BlockType::STONE);
        break;
      case 2:
        changeSelectedBlockType(BlockType::COBBLESTONE);
        break;
      case 3:
        changeSelectedBlockType(BlockType::DIRT);
        break;
      case 4:
        changeSelectedBlockType(BlockType::GRASS);
        break;
      case 5:
        changeSelectedBlockType(BlockType::SAND);
        break;
      case 6:
        changeSelectedBlockType(BlockType::SANDSTONE);
        break;
      case 7:
        changeSelectedBlockType(BlockType::SNOW);
        break;
      case 8:
        changeSelectedBlockType(BlockType::OAK_LOG);
        break;
      case 9:
        changeSelectedBlockType(BlockType::PINE_LOG);
        break;
      case 0:
        changeSelectedBlockType(BlockType::PALM_LOG);
        break;
      }
    }
  }
}
