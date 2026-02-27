#pragma once

struct Settings {
  bool show_debug;
  bool wireframe;
  bool vsync;
  bool fullscreen;
  bool show_cursor;
  float mouse_sensitivity;
  float fov;

  Settings()
      : show_debug(false), wireframe(false), vsync(false), fullscreen(true),
        show_cursor(false), mouse_sensitivity(0.05f), fov(70.0f) {}
};

inline Settings g_settings;
