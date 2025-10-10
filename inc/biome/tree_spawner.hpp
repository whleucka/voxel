#pragma once

#include "biome/tree_generator.hpp"
#include "chunk/chunk.hpp"
#include <functional>
#include <memory>

class TreeSpawner {
public:
  TreeSpawner(double density, std::unique_ptr<TreeGenerator> generator);
  void spawn(Chunk &chunk,
             std::function<bool(Chunk &, int, int, int)> isValidSpawn);

private:
  double density;
  std::unique_ptr<TreeGenerator> generator;
};
