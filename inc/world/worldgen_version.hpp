#pragma once

#include <cstdint>

// Which terrain generator a world was made with. A world keeps its version
// for life: terrain is regenerated from the seed on every load and only edits
// are saved, so changing the generator under an old world would move the
// ground beneath its builds.
namespace worldgen {

constexpr uint8_t kOriginal = 1; // before caves
constexpr uint8_t kCaves = 2;    // natural caves, crystals, pools
constexpr uint8_t kLatest = kCaves;

} // namespace worldgen
