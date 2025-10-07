#pragma once

#include "render/shader.hpp"
#include "shapes/cube_data.hpp"
#include "shapes/cube_mesh.hpp"
#include <glm/glm.hpp>

class Cube {
public:
  Cube()
      : mesh(new CubeMesh(CubeData::vertices, CubeData::indices)),
        shader(new Shader("assets/shaders/basic_color.vert",
                          "assets/shaders/basic_color.frag")) {}

  ~Cube() {
    delete shader;
    delete mesh;
  }

  void uploadGPU() { mesh->generate(); }
  void draw(const glm::mat4 &view, const glm::mat4 &projection) {
    mesh->draw(shader, view, projection);
  }

private:
  CubeMesh *mesh = nullptr;
  Shader *shader = nullptr;
};
