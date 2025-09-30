#pragma once

#include <glad/glad.h>

#include <vector>

#include "mesh.hpp"
#include "shader.hpp"

class CloudManager {
 public:
  CloudManager();
  void generateCloudMesh();
  void render(Shader& shader) const;

 private:
  Mesh m_mesh;
  int m_width;
  int m_depth;
  int m_height;
};
