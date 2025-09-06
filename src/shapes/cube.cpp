#include "shapes/cube.hpp"

namespace Cube {
const std::array<Vertex, 4> faceVertices[6] = {
    // FRONT face (z = +0.5f)
    std::array<Vertex, 4>{{
        {{-0.5f, -0.5f, 0.5f}, {0.f, 0.f, 1.f}, {0.f, 0.f}}, // bottom-left
        {{0.5f, -0.5f, 0.5f}, {0.f, 0.f, 1.f}, {1.f, 0.f}},  // bottom-right
        {{0.5f, 0.5f, 0.5f}, {0.f, 0.f, 1.f}, {1.f, 1.f}},   // top-right
        {{-0.5f, 0.5f, 0.5f}, {0.f, 0.f, 1.f}, {0.f, 1.f}},  // top-left
    }},

    // BACK face (z = -0.5f)
    std::array<Vertex, 4>{{
        {{0.5f, -0.5f, -0.5f}, {0.f, 0.f, -1.f}, {0.f, 0.f}},
        {{-0.5f, -0.5f, -0.5f}, {0.f, 0.f, -1.f}, {1.f, 0.f}},
        {{-0.5f, 0.5f, -0.5f}, {0.f, 0.f, -1.f}, {1.f, 1.f}},
        {{0.5f, 0.5f, -0.5f}, {0.f, 0.f, -1.f}, {0.f, 1.f}},
    }},

    // LEFT face (x = -0.5f)
    std::array<Vertex, 4>{{
        {{-0.5f, -0.5f, -0.5f}, {-1.f, 0.f, 0.f}, {0.f, 0.f}},
        {{-0.5f, -0.5f, 0.5f}, {-1.f, 0.f, 0.f}, {1.f, 0.f}},
        {{-0.5f, 0.5f, 0.5f}, {-1.f, 0.f, 0.f}, {1.f, 1.f}},
        {{-0.5f, 0.5f, -0.5f}, {-1.f, 0.f, 0.f}, {0.f, 1.f}},
    }},

    // RIGHT face (x = +0.5f)
    std::array<Vertex, 4>{{
        {{0.5f, -0.5f, 0.5f}, {1.f, 0.f, 0.f}, {0.f, 0.f}},
        {{0.5f, -0.5f, -0.5f}, {1.f, 0.f, 0.f}, {1.f, 0.f}},
        {{0.5f, 0.5f, -0.5f}, {1.f, 0.f, 0.f}, {1.f, 1.f}},
        {{0.5f, 0.5f, 0.5f}, {1.f, 0.f, 0.f}, {0.f, 1.f}},
    }},

    // TOP face (y = +0.5f)
    std::array<Vertex, 4>{{
        {{-0.5f, 0.5f, 0.5f}, {0.f, 1.f, 0.f}, {0.f, 0.f}},
        {{0.5f, 0.5f, 0.5f}, {0.f, 1.f, 0.f}, {1.f, 0.f}},
        {{0.5f, 0.5f, -0.5f}, {0.f, 1.f, 0.f}, {1.f, 1.f}},
        {{-0.5f, 0.5f, -0.5f}, {0.f, 1.f, 0.f}, {0.f, 1.f}},
    }},

    // BOTTOM face (y = -0.5f)
    std::array<Vertex, 4>{{
        {{-0.5f, -0.5f, -0.5f}, {0.f, -1.f, 0.f}, {0.f, 0.f}},
        {{0.5f, -0.5f, -0.5f}, {0.f, -1.f, 0.f}, {1.f, 0.f}},
        {{0.5f, -0.5f, 0.5f}, {0.f, -1.f, 0.f}, {1.f, 1.f}},
        {{-0.5f, -0.5f, 0.5f}, {0.f, -1.f, 0.f}, {0.f, 1.f}},
    }},
};

// indices for each face (two triangles per quad)
const std::array<unsigned int, 6> faceIndices[6] = {
    std::array<unsigned int, 6>{{0, 1, 2, 2, 3, 0}}, // front
    std::array<unsigned int, 6>{{0, 1, 2, 2, 3, 0}}, // back
    std::array<unsigned int, 6>{{0, 1, 2, 2, 3, 0}}, // left
    std::array<unsigned int, 6>{{0, 1, 2, 2, 3, 0}}, // right
    std::array<unsigned int, 6>{{0, 1, 2, 2, 3, 0}}, // top
    std::array<unsigned int, 6>{{0, 1, 2, 2, 3, 0}}, // bottom
};
} // namespace Cube
