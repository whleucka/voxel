#pragma once

#include "sim/protocol.hpp"
#include <vector>

// Client-side smoothing of entity snapshots. Entities are simulated at 20 Hz
// but drawn every frame, so the client draws them between the last two
// snapshots (about one tick behind the Game), which hides the steps.
class EntityInterpolator {
public:
  // A new snapshot arrived: the previous "current" becomes "previous".
  void push(EntitySnapshot snap);

  // Views blended between the last two snapshots. alpha 0 = previous,
  // 1 = current (clamped). Entities new in the current snapshot appear at
  // their current position; ones missing from it are gone.
  std::vector<EntityView> views(float alpha) const;
  // The same for the other players in the snapshots.
  std::vector<PlayerView> players(float alpha) const;

private:
  EntitySnapshot prev, curr;
  bool have_prev = false;
};

// Players sitting in a boat are drawn in their seat on the boat as drawn,
// facing its way, so they never lag behind it (whatever the snapshots say).
void placeRiders(std::vector<PlayerView> &players, const std::vector<EntityView> &entities);
