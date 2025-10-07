#pragma once
#include <array>
#include <glad/glad.h>
#include <glm/vec2.hpp>
#include <string>

class TextureManager {
public:
  TextureManager();
  ~TextureManager();

  bool loadAtlas(const std::string &path);
  void bind(GLenum texture_unit = GL_TEXTURE0) const;
  std::array<glm::vec2, 4> getQuadUV(int tile_x, int tile_y) const;

  GLuint id() const { return texture_id; }

private:
  GLuint texture_id;
  int atlas_size;
  int tile_size;
  int tiles_per_row;
};
