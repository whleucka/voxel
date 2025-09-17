#pragma once

#include "chunk.hpp"
#include "mesh.hpp"
#include <functional>

class GreedyMesher {
public:
  using SampleFn = std::function<BlockType(int gx, int gy, int gz)>;
  static void build(const Chunk &chunk, SampleFn sample, Mesh &opaque, Mesh &transparent);
};
