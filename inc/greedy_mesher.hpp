#pragma once

#include "chunk.hpp"
#include <functional>
#include <glm/glm.hpp>
#include <vector>

struct CpuMesh {
  std::vector<Vertex> vertices;
  std::vector<unsigned int> indices;
};

class GreedyMesher {
public:
  using SampleFn = std::function<BlockType(int gx, int gy, int gz)>;
  static std::pair<CpuMesh, CpuMesh> build_cpu(const Chunk &chunk,
                                               SampleFn sample);
};
