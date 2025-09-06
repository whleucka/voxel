#pragma once

#include <glad/glad.h>

#include "shader.hpp"
#include "texture.hpp"
#include "vertex.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>

using namespace std;

/**
 * mesh.hpp
 *
 * This mesh class is based on learnopengl.com example
 * Abstract mesh class to draw vertices with GPU
 *
 */
class Mesh {
public:
  vector<Vertex> vertices;
  vector<unsigned int> indices;
  vector<Texture> textures;
  unsigned int VAO;

  Mesh() {} // empty/default mesh

  Mesh(vector<Vertex> v, vector<unsigned int> i, vector<Texture> t)
      : vertices(v), indices(i), textures(t) {
    setupMesh();
  }

  void draw(Shader &shader) {
    // bind appropriate textures
    unsigned int diffuseNr = 1;
    unsigned int specularNr = 1;
    unsigned int normalNr = 1;
    unsigned int heightNr = 1;
    for (unsigned int i = 0; i < textures.size(); i++) {
      glActiveTexture(GL_TEXTURE0 + i);
      // retrieve texture number (the N in diffuse_textureN)
      string number;
      string name = textures[i].type;
      if (name == "texture_diffuse")
        number = std::to_string(diffuseNr++);
      else if (name == "texture_specular")
        number =
            std::to_string(specularNr++); // transfer unsigned int to string
      else if (name == "texture_normal")
        number = std::to_string(normalNr++); // transfer unsigned int to string
      else if (name == "texture_height")
        number = std::to_string(heightNr++); // transfer unsigned int to string

      // now set the sampler to the correct texture unit
      glUniform1i(glGetUniformLocation(shader.ID, (name + number).c_str()), i);
      // and finally bind the texture
      glBindTexture(GL_TEXTURE_2D, textures[i].id);
    }

    // draw mesh
    glBindVertexArray(VAO);
    glDrawElements(GL_TRIANGLES, static_cast<unsigned int>(indices.size()),
                   GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);

    // always good practice to set everything back to defaults once configured.
    glActiveTexture(GL_TEXTURE0);
  }

  void setupMesh() {
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex),
                 vertices.data(), GL_STATIC_DRAW);

    // ⚠️ EBO must be bound while VAO is bound (and NEVER unbound afterward)
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int),
                 indices.data(), GL_STATIC_DRAW);

    // attributes (match vertex.hpp layout)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)0);

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          (void *)offsetof(Vertex, normal));

    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          (void *)offsetof(Vertex, texCoord));

    glBindVertexArray(0); // ✅ unbind the VAO
    // DO NOT: glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);  ❌ leave EBO bound to
    // the VAO
  }

private:
  unsigned int VBO, EBO;
};
