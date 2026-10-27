#pragma once

// The shared look of the title screen, pause menu and settings page.

// Rounded, roomy widgets with warm hover colours. Call once after creating
// the ImGui context; it also tidies the debug panel.
void applyMenuStyle();

// A panel centred horizontally, its centre at y_frac of the display height,
// drawn in the menu font size. Always pair with endMenuPanel().
void beginMenuPanel(const char *id, float width, float y_frac = 0.58f);
void endMenuPanel();

// A full-width menu button; true when pressed.
bool menuButton(const char *label, float height = 38.0f);

// The menu font size (the debug panel keeps the default).
constexpr float kMenuFontSize = 18.0f;
