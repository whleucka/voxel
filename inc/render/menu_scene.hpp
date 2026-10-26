#pragma once

#include "world/game_clock.hpp"
#include <glm/glm.hpp>
#include <memory>
#include <string>

class World;

// The camera behind the title menu circles a spot in the world.
struct OrbitView {
  glm::vec3 eye{0.0f};
  glm::vec3 target{0.0f};
};
constexpr double kOrbitPeriod = 90.0;  // seconds per lap
constexpr float kOrbitRadius = 20.0f;
constexpr float kOrbitHeight = 10.0f;
OrbitView orbitAt(glm::vec3 centre, double seconds);

// A live, read-only view of a saved world behind the menu: no Game, no
// player, nothing saved. World generation reads the global seed, so the
// scene sets it while open and puts it back on close(); close it before a
// game starts.
class MenuScene {
public:
  // A world with this seed is shown when there are no saves yet.
  static constexpr uint32_t kScenicSeed = 20261026;

  MenuScene();
  ~MenuScene();

  // Shows the world saved in save_dir (or the scenic seed if there is none
  // there). Needs a GL context.
  void open(const std::string &save_dir);
  void close();
  bool isOpen() const { return world != nullptr; }

  // Advances the clock and camera and draws the world.
  void frame(float dt, int width, int height);
  // 1 while the scene is still appearing, fading to 0 once chunks are in:
  // how much of the menu colour to draw over it.
  float cover() const { return cover_; }

private:
  std::unique_ptr<World> world;
  GameClock clock;
  glm::vec3 centre{0.0f};
  double seconds = 0.0;
  // One height for the whole lap, above everything on the circle, so the
  // camera glides level instead of climbing and diving with the ground.
  float lap_height = 0.0f;   // where it's heading (rises as chunks load)
  float eye_y = 0.0f;        // where it is, easing toward lap_height
  bool eye_set = false;
  double next_survey = 0.0;
  void surveyLap();
  float cover_ = 1.0f;
  uint32_t saved_seed = 0;
  glm::vec2 saved_offset{0.0f};
};
