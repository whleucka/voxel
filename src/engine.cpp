#include "engine.hpp"
#include <iostream>
#define STB_IMAGE_IMPLEMENTATION
#include "render_ctx.hpp"
#include "stb_image.h"

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
        if (eng->firstMouse) {
          eng->lastX = xpos;
          eng->lastY = ypos;
          eng->firstMouse = false;
        }
        float dx = float(xpos - eng->lastX);
        float dy = float(eng->lastY - ypos);
        eng->lastX = xpos;
        eng->lastY = ypos;
        eng->camera.processMouseMovement(dx, dy);
      });

  // OpenGL settings
  glViewport(0, 0, width, height);
  glDepthRange(0.0, 1.0);
  glClearDepth(1.0);
  glEnable(GL_DEPTH_TEST); // enable depth so faces don’t z-fight
  // Wireframe
  // glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  // Load assets/world
  loadAtlas("res/block_atlas.png");
  blockShader = new Shader("shaders/block.vert", "shaders/block.frag");
  world = new World(5, 5, 5, atlasTex);

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

  atlasTex.id = texId;
  atlasTex.type = "texture_diffuse"; // Mesh::draw expects this exact string
}

void Engine::run() {
  while (!glfwWindowShouldClose(window)) {
    float currentFrame = glfwGetTime();
    deltaTime = currentFrame - lastFrame;
    lastFrame = currentFrame;

    processInput();
    update(deltaTime);
    render();

    glfwSwapBuffers(window);
    glfwPollEvents();
  }
}

void Engine::processInput() {
  if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
    glfwSetWindowShouldClose(window, true);

  if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
    camera.processKeyboard(FORWARD, deltaTime);
  if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
    camera.processKeyboard(BACKWARD, deltaTime);
  if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
    camera.processKeyboard(LEFT, deltaTime);
  if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
    camera.processKeyboard(RIGHT, deltaTime);
  if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
    camera.processKeyboard(UP, deltaTime);
  if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
    camera.processKeyboard(DOWN, deltaTime);
}

void Engine::update(float dt) {
  // TODO: Game logic / world updates
  world->update(dt);
}

void Engine::render() {
  glClearColor(0.0f, 11.0f / 255.0f, 28.0f / 255.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glm::mat4 view = camera.getViewMatrix();
  glm::mat4 proj = glm::perspective(glm::radians(camera.zoom),
                                    float(width) / float(height), 0.1f, 100.0f);

  renderCtx ctx{*blockShader, view, proj};
  world->draw(ctx);
}

void Engine::cleanup() {
  if (atlasTex.id) {
    glDeleteTextures(1, &atlasTex.id);
    atlasTex.id = 0;
  }
  delete blockShader;
  blockShader = nullptr;

  if (window) {
    glfwDestroyWindow(window);
    glfwTerminate();
    window = nullptr;
  }
}
