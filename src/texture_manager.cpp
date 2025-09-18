#include "texture_manager.hpp"
#include "stb_image.h"
#include <glad/glad.h>
#include <iostream>

void TextureManager::loadTexture(const std::string& name,
                                 const std::string& path) {
  int w, h, channels;
  stbi_set_flip_vertically_on_load(true);

  // Force 4 channels so water has alpha
  unsigned char* data = stbi_load(path.c_str(), &w, &h, &channels, STBI_rgb_alpha);
  if (!data) { std::cerr << "Failed to load " << path << "\n"; return; }

  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);

  GLuint tex_id;
  glGenTextures(1, &tex_id);
  glBindTexture(GL_TEXTURE_2D, tex_id);

  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);  // atlas safety

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

  glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8_ALPHA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
  glGenerateMipmap(GL_TEXTURE_2D); // optional with NEAREST

  stbi_image_free(data);
  textures[name] = {tex_id, "texture_diffuse"};
}

Texture &TextureManager::getTexture(const std::string &name) {
  return textures[name];
}
