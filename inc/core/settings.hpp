#pragma once

struct Settings {
  bool show_debug;
  bool wireframe;
  bool vsync;
  bool fullscreen;
  bool show_cursor;

  Settings()
      : show_debug(false), wireframe(false), vsync(false), fullscreen(true),
        show_cursor(false) {}
};

inline Settings g_settings;
