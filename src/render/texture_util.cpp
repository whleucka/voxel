#include "render/texture_util.hpp"
#include <iostream>
#include <stb_image.h>

GLuint loadSrgbTexture(const std::string &path) {
  int w = 0, h = 0, channels = 0;
  stbi_set_flip_vertically_on_load(false);
  unsigned char *data = stbi_load(path.c_str(), &w, &h, &channels, 4);
  if (!data) {
    std::cerr << "[texture] failed to load " << path << "\n";
    return 0;
  }
  GLuint tex = 0;
  glGenTextures(1, &tex);
  glBindTexture(GL_TEXTURE_2D, tex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB_ALPHA, w, h, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, data);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  stbi_image_free(data);
  return tex;
}
