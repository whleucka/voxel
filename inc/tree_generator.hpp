#pragma once

#include "chunk.hpp"

class TreeGenerator {
public:
  virtual void generate(Chunk &chunk, int x, int y, int z) = 0;
  virtual ~TreeGenerator() {}
};
