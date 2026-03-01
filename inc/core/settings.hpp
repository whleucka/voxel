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

  // Clouds
  bool  clouds_enabled;
  float cloud_height;      // world Y of cloud layer
  float cloud_speed;       // drift speed multiplier
  float cloud_opacity;     // base opacity (0..1)

  // Shadows
  bool  shadows_enabled;
  float shadow_distance;   // how far from camera the shadow map covers

  Settings()
      : world_seed(0), noise_offset(0.0f, 0.0f),
        show_debug(false),
        wireframe(false), vsync(false), fullscreen(true), show_cursor(false),
        fov(70.0f), fog_start(80.0f), fog_end(260.0f),
        mouse_sensitivity(0.05f),
        time_scale(72.0f), water_fog_density(0.04f),
        clouds_enabled(true), cloud_height(192.0f),
        cloud_speed(1.0f), cloud_opacity(0.85f),
        shadows_enabled(true), shadow_distance(120.0f) {}
};

inline Settings g_settings;
