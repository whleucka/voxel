#pragma once

struct Settings {
  bool show_debug;
  bool wireframe;
  bool vsync;
  bool fullscreen;
  bool show_cursor;
  float mouse_sensitivity;
  float fov;
  float fog_start;
  float fog_end;
  float time_scale; // in-game seconds per real second (72 ≈ 20 min day)

  Settings()
      : show_debug(false), wireframe(false), vsync(false), fullscreen(true),
        show_cursor(false), mouse_sensitivity(0.05f), fov(70.0f),
        fog_start(80.0f), fog_end(260.0f), time_scale(72.0f) {}
};

inline Settings g_settings;
