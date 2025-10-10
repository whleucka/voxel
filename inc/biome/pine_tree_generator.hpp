#pragma once

#include "biome/tree_generator.hpp"

class PineTreeGenerator : public TreeGenerator {
public:
  void generate(Chunk &chunk, int x, int y, int z) override;
};
