#pragma once

#include <glm/glm.hpp>
#include <functional>
#include <optional>

// Where the picture is taken from. Only what's drawn changes: aiming and
// physics always work from the eyes.
enum class CameraView { First, Behind, Front };

constexpr float kCameraViewDistance = 4.0f;
constexpr float kCameraViewMargin = 0.3f; // kept clear of a block in the way

// What V does.
CameraView nextCameraView(CameraView view);
// In bed or dead, the view is always first person.
CameraView shownCameraView(CameraView chosen, bool in_bed, bool dead);

// The distance to the first solid block along a ray, if there's one in range.
using CameraRay = std::function<std::optional<float>(glm::vec3 origin, glm::vec3 direction,
                                                     float max_distance)>;
// Where the camera sits, as close as it has to be so no block is in the way.
glm::vec3 cameraViewPosition(glm::vec3 eye, glm::vec3 front, CameraView view, const CameraRay &ray);
// The view matrix: along `front` from behind, or back at the face from in front.
glm::mat4 cameraViewMatrix(glm::vec3 position, glm::vec3 front, glm::vec3 up, CameraView view);
