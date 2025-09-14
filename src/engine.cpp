#include "engine.hpp"
#include <GLFW/glfw3.h>
#include <glm/ext/vector_float3.hpp>
#include <iostream>
#define STB_IMAGE_IMPLEMENTATION
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "render_ctx.hpp"
#include "stb_image.h"
#include <sys/resource.h>

glm::vec3 night(0.0f, 11.0f / 255.0f, 28.0f / 255.0f);
glm::vec3 day(0.4f, 0.7f, 1.0f);
glm::vec3 sunset(1.0f, 0.5f, 0.2f);

size_t getMemoryUsage() {
  struct rusage usage;
  getrusage(RUSAGE_SELF, &usage);
  // Defaults to KB (long)
  return usage.ru_maxrss / 1024.0; // MB
}

void keyCallback(GLFWwindow *window, int key, int scancode, int action,
                 int mods) {
  // This is better than processInput, because I don't have to worry about key
  // held vs key pressed, etc
  if (key == GLFW_KEY_F3 && action == GLFW_PRESS) {
    Engine *engine =
        reinterpret_cast<Engine *>(glfwGetWindowUserPointer(window));
    engine->show_stats = !engine->show_stats;
  }
  if (key == GLFW_KEY_F4 && action == GLFW_PRESS) {
    Engine *engine =
        reinterpret_cast<Engine *>(glfwGetWindowUserPointer(window));
    engine->wireframe = !engine->wireframe;
  }
}

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
        eng->camera.processMouseMovement(dx, dy);
      });

  // OpenGL settings
  glViewport(0, 0, width, height);
  glDepthRange(0.0, 1.0);
  glClearDepth(1.0);
  glEnable(GL_DEPTH_TEST); // enable depth so faces don’t z-fight
  glEnable(GL_CULL_FACE);
  glCullFace(GL_BACK);
  glFrontFace(GL_CCW);
  glfwSwapInterval(0); // 0 = disable vsync, 1 = enable vsync

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  // Load assets/world
  loadAtlas("res/block_atlas.png");
  block_shader = new Shader("shaders/block.vert", "shaders/block.frag");
  world = new World(atlas_texture);
  // Load initial chunk
  world->update(camera.getPos());

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

  return true;
}

void Engine::loadAtlas(std::string path) {
  int w, h, channels;
  stbi_set_flip_vertically_on_load(true); // match your UV orientation
  unsigned char *data = stbi_load(path.c_str(), &w, &h, &channels, 0);
  if (!data) {
    std::cerr << "Failed to load atlas: " << path << "\n";
  }
  unsigned int texId;
  glGenTextures(1, &texId);
  glBindTexture(GL_TEXTURE_2D, texId);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                  GL_NEAREST_MIPMAP_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

  GLenum format = channels == 4 ? GL_RGBA : GL_RGB;
  glTexImage2D(GL_TEXTURE_2D, 0, format, w, h, 0, format, GL_UNSIGNED_BYTE,
               data);
  glGenerateMipmap(GL_TEXTURE_2D);
  stbi_image_free(data);

  atlas_texture.id = texId;
  atlas_texture.type =
      "texture_diffuse"; // Mesh::draw expects this exact string
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

  // Camera controls
  if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
    camera.processKeyboard(FORWARD, delta_time);
  if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
    camera.processKeyboard(BACKWARD, delta_time);
  if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
    camera.processKeyboard(LEFT, delta_time);
  if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
    camera.processKeyboard(RIGHT, delta_time);
  if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
    camera.processKeyboard(UP, delta_time);
  if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
    camera.processKeyboard(DOWN, delta_time);
}

void Engine::update() {
  game_clock.update(delta_time);

  // Update world
  world->update(camera.getPos());
}

void Engine::updateSky() {
  float t = game_clock.fractionOfDay(); // 0.0 → 1.0
  // Compute sun direction with phase shift
  float angle_deg = 360.0f * (t - 0.25f);
  float angle_rad = glm::radians(angle_deg);
  glm::vec3 sun_dir = glm::normalize(glm::vec3(
        std::cos(angle_rad),
        std::sin(angle_rad),
        0.3f
        ));

  // Smoothstep helper
  auto smoothstep01 = [](float a, float b, float x) {
    float v = glm::clamp((x - a) / (b - a), 0.0f, 1.0f);
    return v * v * (3.0f - 2.0f * v);
  };

  // Sunrise 06:00–07:00, Sunset 20:00–21:00
  float sunrise = smoothstep01(0.25f, 0.29f, t);
  float sunset  = 1.0f - smoothstep01(0.83f, 0.875f, t);
  float day_factor = sunrise * sunset;

  // Minimum factor so nights aren’t pitch black
  float min_factor = 0.2f;
  day_factor = glm::max(day_factor, min_factor);

  // Ambient
  glm::vec3 night_ambient(0.10f, 0.10f, 0.15f);
  glm::vec3 day_ambient(0.35f, 0.35f, 0.35f);
  glm::vec3 ambient_color = glm::mix(night_ambient, day_ambient, day_factor);

  // Sunlight only active during day_factor
  float sun_strength = day_factor; // drop dependence on raw sun_dir.y

  block_shader->use();
  block_shader->setVec3("lightDir", glm::normalize(sun_dir));
  block_shader->setVec3("ambientColor", ambient_color);
  block_shader->setFloat("sunStrength", sun_strength);

  // Sky
  glm::vec3 night_sky(0.0f, 11.0f / 255.0f, 28.0f / 255.0f);
  glm::vec3 day_sky(0.4f, 0.7f, 1.0f);
  glm::vec3 clear_color = glm::mix(night_sky, day_sky, day_factor);
  glClearColor(clear_color.r, clear_color.g, clear_color.b, 1.0f);
}

void Engine::render() {
  if (wireframe) {
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
  } else {
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
  }

  // Update sun
  updateSky();

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glm::mat4 view = camera.getViewMatrix();
  glm::mat4 proj = glm::perspective(
      glm::radians(camera.zoom), float(width) / float(height),
      0.5f,   // near plane (don’t keep at 0.1 unless you need it)
      1024.0f // far plane (match your chunk render distance)
  );

  renderCtx ctx{*block_shader, view, proj, camera};
  world->draw(ctx);

  GLenum error = glGetError();
  if (error != GL_NO_ERROR) {
    fprintf(stderr, "OpenGL Error after world->draw: 0x%x\n", error);
  }

  stats();
}

void Engine::drawCrosshairImGui() {
  ImVec2 center(ImGui::GetIO().DisplaySize.x * 0.5f,
                ImGui::GetIO().DisplaySize.y * 0.5f);

  ImDrawList *draw_list = ImGui::GetForegroundDrawList();
  float size = 10.0f;
  float thickness = 2.0f;
  ImU32 color = IM_COL32(255, 255, 255, 255);

  // Drawing a crosshair on the screen
  draw_list->AddLine(ImVec2(center.x - size, center.y),
                     ImVec2(center.x + size, center.y), color, thickness);
  draw_list->AddLine(ImVec2(center.x, center.y - size),
                     ImVec2(center.x, center.y + size), color, thickness);
}

void Engine::stats() {
  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();
  if (show_stats) {
    // Per frame updates
    ImGui::Begin("Stats", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
    ImGuiIO &io = ImGui::GetIO();
    ImGui::Text("Game time: %.2d:%.2d", game_clock.hour(), game_clock.minute());
    ImGui::Text("FPS: %.1f", io.Framerate);
    ImGui::Text("Frame time: %.3f ms", 1000.0f / io.Framerate);
    const glm::vec3 pos = camera.getPos();
    ImGui::Text("Camera: (%.2f, %.2f, %.2f)", pos.x, pos.y, pos.z);
    ImGui::Text("Chunks: %i", world->getChunkCount());
    ImGui::Text("Memory usage: %zu MB", getMemoryUsage());
    ImGui::Checkbox("Wireframe mode", &wireframe);
    ImGui::End();
  }
  drawCrosshairImGui();
  ImGui::Render();
  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void Engine::cleanup() {
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();

  if (atlas_texture.id) {
    glDeleteTextures(1, &atlas_texture.id);
    atlas_texture.id = 0;
  }
  delete block_shader;
  block_shader = nullptr;

  if (window) {
    glfwDestroyWindow(window);
    glfwTerminate();
    window = nullptr;
  }
}
