#pragma once

#include "texture.hpp"
#include <string>
#include <unordered_map>

class TextureManager {
public:
  static TextureManager &getInstance() {
    static TextureManager instance;
    return instance;
  }

  Texture &getTexture(const std::string &name);
  void loadTexture(const std::string &name, const std::string &path);

private:
  TextureManager() = default;
  ~TextureManager() = default;
  TextureManager(const TextureManager &) = delete;
  TextureManager &operator=(const TextureManager &) = delete;

  std::unordered_map<std::string, Texture> textures;
};
