#pragma once

#include <string>

/*
 * texture.hpp
 *
 * Defines the Texture struct used across cube, block, and mesh code.
 *
 */
struct Texture {
  unsigned int id;
  std::string type;
};
