#pragma once

#include "mesh.hpp"

class Cube {
public:
  Cube();
  void draw(Shader &shader);

private:
  Mesh mesh;
};