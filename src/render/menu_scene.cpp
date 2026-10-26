#include "render/menu_scene.hpp"
#include "core/constants.hpp"
#include "core/seed.hpp"
#include "core/settings.hpp"
#include "world/world.hpp"
#include "world/world_save.hpp"
#include <algorithm>
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>

OrbitView orbitAt(glm::vec3 centre, double seconds) {
  double a = 2.0 * 3.14159265358979 * std::fmod(seconds, kOrbitPeriod) / kOrbitPeriod;
  glm::vec3 eye = centre + glm::vec3(static_cast<float>(std::cos(a)) * kOrbitRadius,
                                     kOrbitHeight,
                                     static_cast<float>(std::sin(a)) * kOrbitRadius);
  return {eye, centre};
}

MenuScene::MenuScene() = default;
MenuScene::~MenuScene() { close(); }

void MenuScene::open(const std::string &save_dir) {
  close();
  saved_seed = g_settings.world_seed;
  saved_offset = g_settings.noise_offset;

  WorldSave::LevelData level;
  bool saved = WorldSave::loadLevel(save_dir, level);
  if (!saved) {
    level.seed = kScenicSeed;
    level.noise_offset = noiseOffsetFor(kScenicSeed);
    level.time_of_day = 9 * 3600.0f; // a bright morning
  }
  g_settings.world_seed = level.seed;
  g_settings.noise_offset = level.noise_offset;

  world = std::make_unique<World>();
  if (saved) {
    WorldEdits edits;
    WorldSave::loadEdits(save_dir, edits);
    world->setLoadedEdits(std::move(edits));
  }
  WorldSave::PlayerData player;
  centre = saved && WorldSave::loadPlayer(save_dir, player) ? player.position
                                                            : world->findSpawn();
  world->init();
  world->setInterestPoints(std::span(&centre, 1));

  clock.time_of_day = level.time_of_day;
  seconds = 0.0;
  lap_height = centre.y + kOrbitHeight;
  eye_set = false;
  next_survey = 0.0;
  cover_ = 1.0f;
}

void MenuScene::close() {
  if (!world) return;
  world.reset(); // joins its generator threads, which read the seed
  g_settings.world_seed = saved_seed;
  g_settings.noise_offset = saved_offset;
}

// The highest ground anywhere on the camera's circle (in loaded chunks), so
// the lap height clears hills on the far side too. Only ever raises it.
void MenuScene::surveyLap() {
  constexpr int kSamples = 48;
  for (int i = 0; i < kSamples; ++i) {
    glm::vec3 p = orbitAt(centre, kOrbitPeriod * i / kSamples).eye;
    int cx = static_cast<int>(std::floor(p.x / kChunkWidth));
    int cz = static_cast<int>(std::floor(p.z / kChunkDepth));
    if (!world->getChunk(cx, cz)) continue;
    int x = static_cast<int>(std::floor(p.x)), z = static_cast<int>(std::floor(p.z));
    for (int y = kChunkHeight - 1; y >= 0; --y)
      if (world->isSolidBlock(x, y, z)) {
        lap_height = std::max(lap_height, y + 4.0f);
        break;
      }
  }
}

void MenuScene::frame(float dt, int width, int height) {
  if (!world) return;
  seconds += dt;
  clock.scale = g_settings.time_scale;
  clock.update(dt);
  world->update(dt);

  if (seconds >= next_survey) {
    surveyLap();
    next_survey = seconds + 0.5;
  }
  if (!eye_set) eye_y = lap_height, eye_set = true;
  eye_y += (lap_height - eye_y) * std::min(1.0f, dt * 1.5f);

  // Level flight, looking gently down toward the centre (not straight at a
  // point that may be far below on a slope).
  OrbitView v = orbitAt(centre, seconds);
  v.eye.y = eye_y;
  v.target.y = eye_y - kOrbitHeight * 0.6f;

  // Fade in once the ground around the centre has arrived.
  if (world->getChunkCount() > 0) cover_ = std::max(0.0f, cover_ - dt / 1.5f);

  glm::mat4 view = glm::lookAt(v.eye, v.target, glm::vec3(0, 1, 0));
  glm::mat4 proj = glm::perspective(glm::radians(g_settings.fov),
                                    static_cast<float>(width) / std::max(height, 1), 0.1f,
                                    1000.0f);
  world->render(view, proj, clock.fractionOfDay(), width, height, v.eye, false);
}
