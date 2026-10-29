#pragma once

#include <glm/glm.hpp>
#include <optional>

class World;

// How boats float and steer. The driver's client runs steerBoat() and reports
// where the boat got to; the Game checks each move with boatMoveAllowed().
// A boat's position is the middle of its bottom, like an entity's feet, and
// its yaw is an entity's (0 faces -Z, 90 faces +X).

constexpr float kBoatMaxSpeed = 8.0f;     // blocks/s: faster than sprinting
constexpr float kBoatReverseSpeed = 2.0f; // backing up is slow
constexpr float kBoatAccel = 8.0f;        // full speed in about a second
constexpr float kBoatDrag = 8.0f;         // coasts to a stop in about a second
constexpr float kBoatTurnSpeed = 120.0f;  // degrees/s, toward where the driver looks
constexpr float kBoatDraft = 0.15f;       // the hull sits this far below the surface
// Seats: 0 is the driver's, at the back; 1 is the passenger's, in front.
constexpr int kBoatSeats = 2;
constexpr float kBoatSeatHeight = 0.26f;  // the seats' top, above the bottom
constexpr float kBoatSeatOffset = 0.3f;   // each seat's distance from the middle
constexpr float kSeatedEyeHeight = 0.85f; // a rider's eyes above the seat
constexpr float kSeatedHeight = 1.25f;    // a rider's head, above the seat
// The Game refuses a reported move longer than this (a frame at 8 b/s is
// a fraction of it; a lagging frame still fits).
constexpr float kBoatMaxStep = 3.0f;

struct BoatState {
  glm::vec3 position{0.0f};
  float yaw = 0.0f;
  float speed = 0.0f;      // along the facing; negative backs up
  float fall_speed = 0.0f; // off the water: falling
};

struct BoatInput {
  bool forward = false;
  bool back = false;
  float look_yaw = 0.0f; // where the driver looks, as an entity yaw
};

// Which way a boat at this yaw faces, on the ground plane.
glm::vec3 boatForward(float yaw);
// A camera's yaw (0 = +X) as a boat's (0 = -Z).
inline float boatYawFor(float camera_yaw) { return camera_yaw + 90.0f; }
// Where a rider in `seat` sits (the seat's top, in the middle).
glm::vec3 seatPosition(glm::vec3 boat, float yaw, int seat);

// The top of the water a boat at `pos` floats on, if its bottom is about at
// water level: the surface within a block above or below the bottom.
std::optional<float> waterSurfaceAt(const World &world, glm::vec3 pos);
// Where a boat floats on a surface.
inline float floatHeight(float surface) { return surface - kBoatDraft; }
// Floating: its bottom at the waterline.
bool afloat(const World &world, glm::vec3 pos);

// One step of driving. Afloat, W speeds up and turns toward the look, S
// slows and then backs up, and with neither it coasts to a stop. Off the
// water it has no power: it stops, and falls if there's nothing under it.
// It stops at blocks rather than going into them. In unloaded ground it
// holds still.
void steerBoat(BoatState &boat, const BoatInput &input, const World &world, float dt);

// Whether the Game takes a driver's report of the boat moving from `from` to
// `to`: a number, not too far, not climbing, and still on the water (a boat
// off the water may only settle downward).
bool boatMoveAllowed(const World &world, glm::vec3 from, glm::vec3 to);
