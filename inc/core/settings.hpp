#pragma once

#include <glm/vec2.hpp>
#include <cstdint>
#include <string>

struct Settings {
  // World
  std::string world_name;    // save under saves/<world_name>/
  uint32_t   world_seed;
  glm::vec2  noise_offset;  // added to all noise coordinates for seeding
  int        render_distance; // how far (in world units) chunks stay loaded

  // UI
  bool show_debug;

  // Rendering
  int   window_width;  // windowed-mode width  (ignored while fullscreen)
  int   window_height; // windowed-mode height (ignored while fullscreen)
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
  float cloud_height;      // world Y of the cloud band's base
  int   cloud_thickness;   // band depth in cells — how many layers deep clouds stack
  float cloud_speed;       // drift speed multiplier
  float cloud_opacity;     // base opacity (0..1)

  // Shadows
  bool  shadows_enabled;
  float shadow_distance;   // how far from camera the shadow map covers
  int   shadow_map_size;   // depth texture resolution (1024/2048/4096)

  Settings()
      : world_name("world"),
        world_seed(0), noise_offset(0.0f, 0.0f), render_distance(320),
        show_debug(false),
        window_width(1600), window_height(900),
        wireframe(false), vsync(false), fullscreen(true), show_cursor(false),
        fov(70.0f), fog_start(80.0f), fog_end(260.0f),
        mouse_sensitivity(0.05f),
        time_scale(35.0f), water_fog_density(0.04f),
        clouds_enabled(true), cloud_height(192.0f), cloud_thickness(8),
        cloud_speed(1.0f), cloud_opacity(0.85f),
        shadows_enabled(true), shadow_distance(120.0f),
        shadow_map_size(2048) {}
};

inline Settings g_settings;
