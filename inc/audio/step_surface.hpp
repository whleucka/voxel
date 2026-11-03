#pragma once

#include "block/block_type.hpp"
#include <cstddef>
#include <optional>

// Which footstep sounds a block makes underfoot.
enum class StepSet { Grass = 0, Dirt, Stone, Wood, Snow, Cloth, Silent };
inline constexpr std::size_t kStepSetCount = 6; // the ones with sounds

// Nothing for a block missing from the table (a test checks there are none);
// stepSurface() makes those silent.
std::optional<StepSet> listedStepSurface(BlockType b);
inline StepSet stepSurface(BlockType b) { return listedStepSurface(b).value_or(StepSet::Silent); }
