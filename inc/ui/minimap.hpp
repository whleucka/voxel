#pragma once

#include "block/block_type.hpp"
#include <array>
#include <glm/glm.hpp>

class World;

// The minimap's picture, as plain data: update() samples the loaded world
// around the player and the Hud draws it, turned so that straight ahead is
// up. One cell is one block column.
//
// Out in the open it shows the top block of each column, lit by its height.
// Under rock it shows a slice at the player's feet instead: open space light
// and rock dark, so the way back out of a cave shows.
class Minimap {
public:
  static constexpr int kRadius = 32; // blocks from the middle to the rim
  static constexpr int kSize = 2 * kRadius + 1;
  // How long a new view has to hold before the map switches to it, so a
  // short overhang doesn't make it flicker.
  static constexpr float kSwitchSeconds = 0.75f;
  // How often the picture is redrawn while standing still (blocks change).
  static constexpr float kRefreshSeconds = 0.5f;

  enum class View : uint8_t { Surface, Cave };

  // Switches view if it's time, and resamples when the player has moved a
  // block or the picture is getting old.
  void update(const World &world, glm::vec3 feet, float dt);
  // Resamples straight away, around `centre`, in `view`.
  void sample(const World &world, glm::ivec3 centre, View view);

  View view() const { return view_; }
  glm::ivec3 centre() const { return centre_; }
  // The cell `dx` blocks east and `dz` south of the middle; alpha 0 where
  // nothing is loaded. Outside the map: nothing.
  glm::u8vec4 cell(int dx, int dz) const;

private:
  View view_ = View::Surface;
  View pending_ = View::Surface; // the view the player is in now
  float pending_s_ = 0.0f;       // how long they've been in it
  float since_sample_ = kRefreshSeconds;
  bool sampled_ = false;
  glm::ivec3 centre_{0};
  std::array<glm::u8vec4, kSize * kSize> cells_{};
};

// A block's colour seen from above (alpha 0 for air).
glm::u8vec4 mapColour(BlockType type);

// Whether the player at `feet` is under rock: little of the sky's light
// reaches their head. Deep water doesn't count, nor a chunk not loaded yet.
bool underRock(const World &world, glm::vec3 feet);

// Where something `delta` blocks away (x east, y south) sits on the map, in
// blocks from the middle (x right, y down), for a player facing `yaw` degrees
// (the camera's: -90 looks north, along -Z).
glm::vec2 mapOffset(glm::vec2 delta, float yaw);

// A marker at map offset `offset`: inside the rim it's drawn where it is;
// further away it sits on the rim, pointing the way.
struct MarkerSpot {
  glm::vec2 at{0.0f};
  bool on_rim = false;
};
MarkerSpot placeMarker(glm::vec2 offset, float radius);
