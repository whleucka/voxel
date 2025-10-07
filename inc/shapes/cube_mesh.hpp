#pragma once

#include "render/shader.hpp"
#include "shapes/cube_vertex.hpp"
#include <cstddef>
#include <vector>
#include <span>
#include <glm/ext/matrix_transform.hpp>

class CubeMesh {
public:
  CubeMesh(std::span<const CubeVertex> v,
         std::span<const uint32_t> i)
    : vertices(v.begin(), v.end()), indices(i.begin(), i.end()) {}

  ~CubeMesh() {
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &EBO);
  }

  void generate() {
    // Generate VAO + VBO + EBO
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);

    // Vertex buffer
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(CubeVertex), &vertices[0], GL_STATIC_DRAW);

    // Element buffer
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), &indices[0], GL_STATIC_DRAW);

    // Position pointer
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(CubeVertex), (void *)0);
    // Colour pointer
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(CubeVertex), (void *)offsetof(CubeVertex, colour));

    // Unbind VBO
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
  }

  void draw(Shader *shader, glm::mat4 view, glm::mat4 projection) {
    glm::mat4 model = glm::mat4(1.0f);
    glm::vec3 pos = glm::vec3(0, 0, -3);
    model = glm::translate(model, pos);

    shader->use();
    shader->setMat4("model", model);
    shader->setMat4("view", view);
    shader->setMat4("projection", projection);

    glBindVertexArray(VAO);
    glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_INT, 0);
  }

private:
  unsigned int VAO, VBO, EBO = 0;
  std::vector<CubeVertex> vertices;
  std::vector<uint32_t> indices;
};
