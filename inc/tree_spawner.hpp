#pragma once

#include "chunk.hpp"
#include "tree_generator.hpp"
#include <memory>
#include <functional>

class TreeSpawner {
public:
  TreeSpawner(double density, std::unique_ptr<TreeGenerator> generator);
  void spawn(Chunk &chunk, std::function<bool(Chunk &, int, int, int)> isValidSpawn);

private:
  double m_density;
  std::unique_ptr<TreeGenerator> m_generator;
};


