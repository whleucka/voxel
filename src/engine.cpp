#include "engine.hpp"
#include <glm/ext/vector_float3.hpp>
#include <iostream>
#define STB_IMAGE_IMPLEMENTATION
#include "render_ctx.hpp"
#include "stb_image.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

bool Engine::init() {
  if (!glfwInit()) {
    std::cerr << "Failed to initialize GLFW\n";
    return false;
  }

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

  window = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
  if (!window) {
    std::cerr << "Failed to create GLFW window\n";
    glfwTerminate();
    return false;
  }

  glfwMakeContextCurrent(window);
  glfwSetWindowUserPointer(window, this);

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
  // Wireframe
  // glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  // Load assets/world
  loadAtlas("res/block_atlas.png");
  block_shader = new Shader("shaders/block.vert", "shaders/block.frag");
  world = new World(atlas_texture);
  // Load initial chunk
  world->update(0.0f, camera.getPos());

  // ImGui
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

  ImGui::StyleColorsDark();

  // Platform/Renderer bindings
  ImGui_ImplGlfw_InitForOpenGL(window, true);
  ImGui_ImplOpenGL3_Init("#version 330"); // or "#version 130" depending on context

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
  atlas_texture.type = "texture_diffuse"; // Mesh::draw expects this exact string
}

void Engine::run() {
  while (!glfwWindowShouldClose(window)) {
    float currentFrame = glfwGetTime();
    delta_time = currentFrame - last_frame;
    last_frame = currentFrame;

    processInput();
    update(delta_time);
    render();

    glfwSwapBuffers(window);
    glfwPollEvents();
  }
}

void Engine::processInput() {
  if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
    glfwSetWindowShouldClose(window, true);

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

void Engine::update(float dt) {
  world->update(dt, camera.getPos());
}

void Engine::render() {
  // Per frame updates
  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();
  glClearColor(0.0f, 11.0f / 255.0f, 28.0f / 255.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glm::mat4 view = camera.getViewMatrix();
  glm::mat4 proj = glm::perspective(
    glm::radians(camera.zoom),
    float(width) / float(height),
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

void Engine::stats() {
  ImGui::Begin("Stats", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
  ImGuiIO& io = ImGui::GetIO();
  ImGui::Text("FPS: %.1f", io.Framerate);
  ImGui::Text("Frame time: %.3f ms", 1000.0f / io.Framerate);
  const glm::vec3 pos = camera.getPos();
  ImGui::Text("Camera: (x %.2f, y %.2f, z %.2f)", pos.x, pos.y, pos.z);
  ImGui::Text("Chunks: %i", world->getChunkCount());
  ImGui::End();
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
