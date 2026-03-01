#pragma once

#include <glm/vec2.hpp>
#include <cstdint>

struct Settings {
  // World
  uint32_t   world_seed;
  glm::vec2  noise_offset; // added to all noise coordinates for seeding

  // UI
  bool show_debug;

  // Rendering
  bool  wireframe;
  bool  vsync;
  bool  fullscreen;
  bool  show_cursor;
  float fov;
  float fog_start;
  float fog_end;

  // Controls
  float mouse_sensitivity;

  // Atmosphere
  float time_scale;        // in-game seconds per real second (72 ≈ 20 min day)
  float water_fog_density; // underwater exponential fog density

  Settings()
      : world_seed(0), noise_offset(0.0f, 0.0f),
        show_debug(false),
        wireframe(false), vsync(false), fullscreen(true), show_cursor(false),
        fov(70.0f), fog_start(80.0f), fog_end(260.0f),
        mouse_sensitivity(0.05f),
        time_scale(72.0f), water_fog_density(0.04f) {}
};

inline Settings g_settings;
