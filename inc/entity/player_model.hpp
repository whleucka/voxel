#pragma once

#include "entity/entity_type.hpp"
#include <glm/glm.hpp>
#include <optional>
#include <string_view>
#include <vector>

// How other players look: a blocky figure whose shirt colour comes from their
// name, so each player is recognisable and keeps their colour across games.
constexpr int kPlayerColors = 8;
const glm::vec3 &playerColor(std::string_view name);
// The figure (1.8 tall, 0.6 wide, facing -Z), legs and arms swinging.
std::vector<ModelBox> playerModel(glm::vec3 shirt);
// The same figure sitting: hips at the feet position (a seat's top), legs
// out in front, arms down, nothing swinging.
std::vector<ModelBox> seatedPlayerModel(glm::vec3 shirt);
// PlayerView::yaw is the camera's (0 = +X, -90 = -Z); models use 0 = -Z.
inline float playerModelYaw(float camera_yaw) { return camera_yaw + 90.0f; }

// Where a world point lands on screen (pixels, origin top-left), or nothing
// if it is behind the camera. For name tags.
std::optional<glm::vec2> projectToScreen(glm::vec3 point, const glm::mat4 &view_projection,
                                         float width, float height);
