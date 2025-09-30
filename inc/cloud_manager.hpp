// cloud_manager.hpp
#pragma once
#include "mesh.hpp"
#include <glm/glm.hpp>
#include <unordered_map>

class CloudManager {
public:
    CloudManager();

    void update(const glm::vec3& cameraPos);
    void render(Shader& shader) const;

private:
    int m_width, m_depth, m_height;
    int renderDistance;

    struct CloudChunk {
        int cx, cz;
        Mesh mesh;
    };

    std::unordered_map<long long, CloudChunk> chunks;

    void generateChunk(int cx, int cz, Mesh& mesh);
    void emitCloudCell(float x, float yBottom, float yTop, float z,
                   float cellSize,
                   std::vector<Vertex>& verts,
                   std::vector<unsigned int>& inds);
    static long long makeKey(int cx, int cz) {
        return (static_cast<long long>(cx) << 32) ^ (cz & 0xffffffff);
    }
};
