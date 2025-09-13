#include "../inc/camera.hpp"
#include <glm/gtc/matrix_transform.hpp>

void Camera::getFrustumPlanes(glm::vec4 planes[6], float aspect, float near, float far) const {
    glm::mat4 proj = glm::perspective(glm::radians(zoom), aspect, near, far);
    glm::mat4 view = getViewMatrix();
    glm::mat4 clip = proj * view;

    // Extract frustum planes from the clip matrix
    // Right plane
    planes[0].x = clip[0][3] - clip[0][0];
    planes[0].y = clip[1][3] - clip[1][0];
    planes[0].z = clip[2][3] - clip[2][0];
    planes[0].w = clip[3][3] - clip[3][0];

    // Left plane
    planes[1].x = clip[0][3] + clip[0][0];
    planes[1].y = clip[1][3] + clip[1][0];
    planes[1].z = clip[2][3] + clip[2][0];
    planes[1].w = clip[3][3] + clip[3][0];

    // Bottom plane
    planes[2].x = clip[0][3] + clip[0][1];
    planes[2].y = clip[1][3] + clip[1][1];
    planes[2].z = clip[2][3] + clip[2][1];
    planes[2].w = clip[3][3] + clip[3][1];

    // Top plane
    planes[3].x = clip[0][3] - clip[0][1];
    planes[3].y = clip[1][3] - clip[1][1];
    planes[3].z = clip[2][3] - clip[2][1];
    planes[3].w = clip[3][3] - clip[3][1];

    // Far plane
    planes[4].x = clip[0][3] - clip[0][2];
    planes[4].y = clip[1][3] - clip[1][2];
    planes[4].z = clip[2][3] - clip[2][2];
    planes[4].w = clip[3][3] - clip[3][2];

    // Near plane
    planes[5].x = clip[0][3] + clip[0][2];
    planes[5].y = clip[1][3] + clip[1][2];
    planes[5].z = clip[2][3] + clip[2][2];
    planes[5].w = clip[3][3] + clip[3][2];

    // Normalize the planes
    for (int i = 0; i < 6; ++i) {
        float length = glm::length(glm::vec3(planes[i]));
        planes[i] /= length;
    }
}

void Camera::processMouseMovement(float xoffset, float yoffset, bool constrainPitch) {
    xoffset *= mouse_sensitivity;
    yoffset *= mouse_sensitivity;
    yaw   += xoffset;       // turn head left/right
    pitch += yoffset;       // look up/down

    if (constrainPitch) {
      if (pitch >  89.0f) pitch =  89.0f;
      if (pitch < -89.0f) pitch = -89.0f;
    }
    updateCameraVectors();
}
