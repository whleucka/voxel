#pragma once

#include "tree_generator.hpp"

class OakTreeGenerator : public TreeGenerator {
public:
  void generate(Chunk &chunk, int x, int y, int z) override;
};
