#include "texture_manager.hpp"
#include "stb_image.h"
#include <glad/glad.h>
#include <iostream>

void TextureManager::loadTexture(const std::string &name,
                                 const std::string &path) {
  int w, h, channels;
  stbi_set_flip_vertically_on_load(true);
  unsigned char *data = stbi_load(path.c_str(), &w, &h, &channels, 0);
  if (!data) {
    std::cerr << "Failed to load texture: " << path << "\n";
    return;
  }

  unsigned int tex_id;
  glGenTextures(1, &tex_id);
  glBindTexture(GL_TEXTURE_2D, tex_id);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

  GLenum format = channels == 4 ? GL_RGBA : GL_RGB;
  glTexImage2D(GL_TEXTURE_2D, 0, format, w, h, 0, format, GL_UNSIGNED_BYTE,
               data);
  // Add gutters to atlas
  // glGenerateMipmap(GL_TEXTURE_2D);
  stbi_image_free(data);

  textures[name] = {tex_id, "texture_diffuse"};
}

Texture &TextureManager::getTexture(const std::string &name) {
  return textures[name];
}
