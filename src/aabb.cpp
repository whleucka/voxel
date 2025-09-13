#include "../inc/aabb.hpp"
#include <glm/gtc/matrix_transform.hpp>

// Function to check if AABB intersects with a frustum (defined by 6 planes)
bool AABB::intersectsFrustum(const glm::vec4 planes[6]) const {
    // For each plane, check if the AABB is on the positive side of the plane
    // If the AABB is completely outside any plane, it's not intersecting
    for (int i = 0; i < 6; ++i) {
        // p-vertex: vertex of the AABB that is furthest in the positive direction of the plane normal
        // n-vertex: vertex of the AABB that is furthest in the negative direction of the plane normal
        glm::vec3 p_vertex;
        glm::vec3 n_vertex;

        // Determine p-vertex and n-vertex based on the plane normal
        if (planes[i].x >= 0) {
            p_vertex.x = max.x;
            n_vertex.x = min.x;
        } else {
            p_vertex.x = min.x;
            n_vertex.x = max.x;
        }
        if (planes[i].y >= 0) {
            p_vertex.y = max.y;
            n_vertex.y = min.y;
        } else {
            p_vertex.y = min.y;
            n_vertex.y = max.y;
        }
        if (planes[i].z >= 0) {
            p_vertex.z = max.z;
            n_vertex.z = min.z;
        } else {
            p_vertex.z = min.z;
            n_vertex.z = max.z;
        }

        // If p-vertex is on the negative side of the plane, the AABB is outside
        if (glm::dot(glm::vec3(planes[i]), p_vertex) + planes[i].w < 0) {
            return false;
        }
    }
    return true;
}
