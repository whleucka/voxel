#include "core/config.hpp"
#include "core/constants.hpp"
#include "core/engine.hpp"
#include "core/settings.hpp"

int main() {
  // Load voxel.properties (writing a default file on first run) before the
  // engine reads any settings.
  loadServerProperties();

  Engine engine(g_settings.window_width, g_settings.window_height, kAppTitle);
  if (!engine.init())
    return -1;
  engine.run();
  return 0;
}
