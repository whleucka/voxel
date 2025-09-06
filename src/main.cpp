#include "engine.hpp"
#include <iostream>

void glfw_error_callback(int error, const char* description) {
    std::cerr << "GLFW Error (" << error << "): " << description << std::endl;
}

int main() {
  glfwSetErrorCallback(glfw_error_callback);
  Engine engine(1280, 720, "Blocks");
  if (!engine.init())
    return -1;
  engine.run();
  return 0;
}
