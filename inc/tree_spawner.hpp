#pragma once

#include "chunk.hpp"

class TreeSpawner {
public:
  TreeSpawner(double density);
  void spawn(Chunk &chunk);

private:
  double m_density;
};


