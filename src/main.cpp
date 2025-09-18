#include "engine.hpp"
#include <iostream>
#include <GLFW/glfw3.h>

void glfw_error_callback(int error, const char* description) {
    std::cerr << "GLFW Error (" << error << "): " << description << std::endl;
}

int main() {
  glfwSetErrorCallback(glfw_error_callback);
  Engine engine(800, 600, "Voxel 3D");
  if (!engine.init())
    return -1;
  engine.run();
  return 0;
}
