#pragma once
#include <glad/glad.h>
#include <vector>
#include <cstdint>
#include "shader.hpp"
#include "vertex.hpp"

class Mesh {
public:
  Mesh() : VAO(0), VBO(0), EBO(0), indexCount(0) {}

  // upload CPU data to GPU (safe with empty vectors)
  void setupMesh() {
    if (VAO == 0) glGenVertexArrays(1, &VAO);
    if (VBO == 0) glGenBuffers(1, &VBO);
    if (EBO == 0) glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER,
        vertices.empty() ? 0 : vertices.size() * sizeof(Vertex),
        vertices.empty() ? nullptr : vertices.data(),
        GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
        indices.empty() ? 0 : indices.size() * sizeof(uint32_t),
        indices.empty() ? nullptr : indices.data(),
        GL_STATIC_DRAW);

    indexCount = static_cast<GLsizei>(indices.size());

    // attribs
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
        (void*)offsetof(Vertex, position));

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
        (void*)offsetof(Vertex, normal));

    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
        (void*)offsetof(Vertex, text_coord));

    glBindVertexArray(0);
  }

  // draw only if we have indices
  void draw(Shader& shader) const {
    if (VAO == 0 || indexCount == 0) return;
    (void)shader; // not used directly here; renderer binds shader & textures
    glBindVertexArray(VAO);
    glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
  }

  // optional helper to reset GPU buffers when empty
  void clear() {
    vertices.clear();
    indices.clear();
    indexCount = 0;
    // keep VAO/VBO/EBO alive; safe to reuse
  }

  std::vector<Vertex> vertices;
  std::vector<uint32_t> indices;

private:
  GLuint VAO, VBO, EBO;
  GLsizei indexCount;
};
