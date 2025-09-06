#include "engine.hpp"
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#define STB_IMAGE_IMPLEMENTATION
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
    } // so your projection uses the real size
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
        float dy = float(eng->lastY - ypos); // invert once here
        eng->lastX = xpos;
        eng->lastY = ypos;
        eng->camera.processMouseMovement(dx, dy); // no extra negation inside
      });

  glViewport(0, 0, width, height);
  glDepthRange(0.0, 1.0);
  glClearDepth(1.0);
  glEnable(GL_DEPTH_TEST); // enable depth so faces don’t z-fight
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  blockShader = new Shader("shaders/block.vert", "shaders/block.frag");
  testBlock = new Block(BlockType::GRASS, glm::vec3(0.0f), 16);

  {
    int w, h, channels;
    stbi_set_flip_vertically_on_load(true); // match your UV orientation
    unsigned char *data =
        stbi_load("res/block_atlas.png", &w, &h, &channels, 0);
    if (!data) {
      std::cerr << "Failed to load atlas: res/block_atlas.png\n";
      return false;
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

    // attach atlas to the block’s mesh
    testBlock->mesh.textures.clear();
    testBlock->mesh.textures.push_back(atlasTex);

    // upload VAO/VBO/EBO now that vertices/indices are ready
    // Mesh builds in ctor, but we rebuilt data in Block::generate().
    // Call the same setup the Mesh ctor would do (your Mesh keeps it private),
    // so the simplest approach is to reconstruct Mesh with the current data:
    testBlock->mesh = Mesh(testBlock->mesh.vertices, testBlock->mesh.indices,
                           testBlock->mesh.textures);
  }

  return true;
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
}

void Engine::update(float dt) {
  // TODO: Game logic / world updates
}

void Engine::render() {
  glClearColor(0.0f, 11.0f / 255.0f, 28.0f / 255.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glm::mat4 model(1.0f);
  glm::mat4 view = camera.getViewMatrix();
  glm::mat4 proj = glm::perspective(glm::radians(camera.zoom),
                                    float(width) / float(height), 0.1f, 100.0f);

  glUseProgram(blockShader->ID);
  glUniformMatrix4fv(glGetUniformLocation(blockShader->ID, "model"), 1,
                     GL_FALSE, glm::value_ptr(model));
  glUniformMatrix4fv(glGetUniformLocation(blockShader->ID, "view"), 1, GL_FALSE,
                     glm::value_ptr(view));
  glUniformMatrix4fv(glGetUniformLocation(blockShader->ID, "projection"), 1,
                     GL_FALSE, glm::value_ptr(proj));

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, atlasTex.id);
  glUniform1i(glGetUniformLocation(blockShader->ID, "texture_diffuse1"), 0);

  // Wireframe
  // glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

  testBlock->mesh.draw(*blockShader);
}

void Engine::cleanup() {
  if (atlasTex.id) {
    glDeleteTextures(1, &atlasTex.id);
    atlasTex.id = 0;
  }
  delete testBlock;
  testBlock = nullptr;
  delete blockShader;
  blockShader = nullptr;

  if (window) {
    glfwDestroyWindow(window);
    glfwTerminate();
    window = nullptr;
  }
}
