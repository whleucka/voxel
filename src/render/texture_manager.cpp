#include "render/texture_manager.hpp"
#include <iostream>
#include <stb_image.h>

TextureManager::TextureManager()
    : texture_id(0), atlas_size(512), tile_size(32), tiles_per_row(16) {}

TextureManager::~TextureManager() {
  if (texture_id != 0) {
    glDeleteTextures(1, &texture_id);
  }
}

bool TextureManager::loadAtlas(const std::string &path) {
  int width = 0, height = 0, channels = 0;
  stbi_set_flip_vertically_on_load(false); // keep top-left as top-left
  unsigned char *data = stbi_load(path.c_str(), &width, &height, &channels, 4);

  if (!data) {
    std::cerr << "Failed to load texture atlas: " << path << "\n";
    return false;
  }

  if (width != atlas_size || height != atlas_size) {
    std::cerr << "Warning: atlas size mismatch. "
              << "Loaded " << width << "x" << height << ", expected "
              << atlas_size << "x" << atlas_size << "\n";
  }

  glGenTextures(1, &texture_id);
  glBindTexture(GL_TEXTURE_2D, texture_id);

  // Upload to GPU
  glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB_ALPHA, width, height, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, data);

  glGenerateMipmap(GL_TEXTURE_2D);

  // Filtering
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                  GL_NEAREST_MIPMAP_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

  // Wrapping
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  stbi_image_free(data);
  return true;
}

void TextureManager::bind(GLenum texture_unit) const {
  glActiveTexture(texture_unit);
  glBindTexture(GL_TEXTURE_2D, texture_id);
}

std::array<glm::vec2, 4> TextureManager::getQuadUV(int tile_x,
                                                   int tile_y) const {
  float step = static_cast<float>(tile_size) / static_cast<float>(atlas_size);

  float u0 = tile_x * step;
  float u1 = u0 + step;

  float v0 = tile_y * step;
  float v1 = v0 + step;

  // Flip vertically to fix upside-down textures
  return {{
      {u0, v0}, // TL
      {u1, v0}, // TR
      {u1, v1}, // BR
      {u0, v1}  // BL
  }};
}
