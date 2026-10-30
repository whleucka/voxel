#include "core/frame_profile.hpp"
#include <algorithm>

const char *FrameProfile::name(Stage s) {
  switch (s) {
    case Update:     return "Input + move";
    case Game:       return "Game";
    case Uploads:    return "Chunk uploads";
    case Scene:      return "Scene";
    case Hud:        return "HUD";
    case Swap:       return "Present / vsync";
    case GpuScene:   return "GPU scene";
    case GpuShadows: return "  of which shadows";
    case GpuHud:     return "GPU HUD";
    case kStages:    break;
  }
  return "?";
}

void FrameProfile::endFrame() {
  frames[next] = current;
  current = {};
  next = (next + 1) % kWindow;
  filled = std::min(filled + 1, kWindow);
}

double FrameProfile::average(Stage s) const {
  if (filled == 0) return 0.0;
  double sum = 0.0;
  for (int i = 0; i < filled; ++i) sum += frames[i][s];
  return sum / filled;
}

double FrameProfile::worst(Stage s) const {
  double most = 0.0;
  for (int i = 0; i < filled; ++i) most = std::max(most, frames[i][s]);
  return most;
}
