#include "shapes/cube.hpp"
#include <vector>

Cube::Cube() {
  std::vector<Vertex> vertices = {
      // Front face
      {{-0.5f, -0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},
      {{0.5f, -0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}},
      {{0.5f, 0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
      {{-0.5f, 0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},
      // Back face
      {{-0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {1.0f, 0.0f}},
      {{0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {0.0f, 0.0f}},
      {{0.5f, 0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {0.0f, 1.0f}},
      {{-0.5f, 0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {1.0f, 1.0f}},
  };

  std::vector<unsigned int> indices = {
      // Front face
      0, 1, 2, 0, 2, 3,
      // Back face
      4, 5, 6, 4, 6, 7,
      // Left face
      4, 0, 3, 4, 3, 7,
      // Right face
      1, 5, 6, 1, 6, 2,
      // Top face
      3, 2, 6, 3, 6, 7,
      // Bottom face
      0, 1, 5, 0, 5, 4
  };

  mesh.vertices = vertices;
  mesh.indices = indices;
  mesh.setupMesh();
}

void Cube::draw(Shader &shader) { mesh.draw(shader); }