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

// indices for each face (two triangles per quad, CCW winding)
const std::array<unsigned int, 6> faceIndices[6] = {
    {0, 1, 2, 0, 2, 3}, // front
    {0, 1, 2, 0, 2, 3}, // back
    {0, 1, 2, 0, 2, 3}, // left
    {0, 1, 2, 0, 2, 3}, // right
    {0, 1, 2, 0, 2, 3}, // top
    {0, 1, 2, 0, 2, 3}, // bottom
};
} // namespace Cube
