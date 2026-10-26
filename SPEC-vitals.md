# Spec: vitals

Module of [CAPABILITY_MAP.md](CAPABILITY_MAP.md). Depends on: `sim-core`, `hud`.
Used by: `enemies` (damage), `animals` (food, later).

## Objective

Give players health and stamina so the world has stakes. Falling too far and
staying underwater hurt you, sprinting tires you out, health slowly comes
back, and dying respawns you with your items. It stays gentle for young
players.

### Acceptance criteria

The `Game` tracks all vitals in half-point units at its 20 Hz tick.

- **Health:**
  - 0..20, shown as 10 hearts with half-hearts.
  - **Damage** comes from falling and drowning, and later from enemies.
  - **Regen:** after 4 s without damage, +1 every 2 s up to 20.
- **Stamina:**
  - 0..20, shown as 10 bolts with half-bolts.
  - **Sprinting** drains 2 per second. Fly mode doesn't drain it.
  - **Recovery:** after 1 s of not sprinting, it refills at 4 per second.
  - **Exhaustion:** at 0 the player can't sprint until stamina is back to 6. Movement is client-side, so `Game` tells the client.
- **Fall damage:**
  - The damage is `floor(fall distance − 3)`, where fall distance is the highest point of an airborne stretch minus the landing height.
  - Landing in water, or any stretch spent in fly mode, does no damage and resets the fall.
- **Drowning:**
  - Breath is 0..20. It drains 2 per second while your eyes are underwater (10 s in total) and refills at 10 per second otherwise.
  - At 0 you take 2 damage (one heart) per second.
  - The bubbles show only while breath is below full.
- **Death and respawn:**
  - At 0 health the player is dead for 3 s: a "You died" overlay shows, and movement, breaking and placing are ignored.
  - Then they respawn at the world spawn with full vitals and their **whole inventory kept**.
- **HUD:**
  - Hearts sit on the left of the reserved strip above the hotbar, and bolts on the right.
  - Breath bubbles go above the hearts.
  - Taking damage briefly flashes the screen edges red.
- **Persistence:** vitals aren't saved. Loading a world starts you at full health. The save files are unchanged.

## Design

```
Engine (client)                              Game (authoritative)
───────────────                              ─────────────────────
Player physics ── MovePlayer{…, on_ground,   PlayerState.vitals : Vitals   (pure)
                  sprinting, in_water,       PlayerState.fall   : FallTracker (pure)
                  eyes_underwater} ────────► tick(): vitals.tick(activity), fall damage,
                                                    death timer → respawn
vitals view + HUD icons ◄─ VitalsChanged{health, stamina, breath, can_sprint, dead}
Player::setSprintAllowed ◄┘     (to the owner, whenever a shown value changes)
teleport + reset velocity ◄─ Respawned{position}
```

```cpp
// vitals/vitals.hpp: pure, unit-tested, no Game/World
struct Activity { bool sprinting, eyes_underwater, fly; };

class Vitals {
public:
  static constexpr int kMax = 20;
  int health() const; int stamina() const; int breath() const;
  bool canSprint() const; bool dead() const;
  void tick(float dt, const Activity &a);   // drain/regen/drown
  void damage(int amount);                  // resets the regen delay
  void reset();                             // full everything (respawn)
};

class FallTracker {  // highest point of the current airborne stretch
public:
  // Feed every reported position; returns damage on landing (0 otherwise).
  int update(float y, bool on_ground, bool in_water, bool fly);
};
```

- **Fractional rates are accumulated** so the integer values change at
  exact, testable times. For example, 20 s of sprinting drains exactly 40
  points, but stamina stops at 0.
- **Protocol:**
  - `MovePlayer` gains `on_ground`, `sprinting`, `in_water` (feet or eyes) and `eyes_underwater`, all reported by the client.
  - New events: `VitalsChanged` (sent after `Join` and whenever a value changes) and `Respawned {position}`.
- **`Game` integration:**
  - `MovePlayer` feeds `FallTracker`, and fall damage is applied at once.
  - `Game::tick` calls `Vitals::tick`, applies drowning damage, and runs the death timer and respawn.
  - Dead players' `MovePlayer`, `BreakBlock` and `PlaceBlock` are ignored, without a rejection.
- **Respawn point:** the world spawn is computed once with `findSpawn()` and cached in `Game`.
- **Client:**
  - Keeps a vitals view from `VitalsChanged`.
  - Calls `Player::setSprintAllowed(can_sprint)`.
  - On `Respawned`, sets the player's position, zeroes their velocity and clears the fall state.
  - Draws the death overlay while `dead` is set.
- **HUD:**
  - `HudLayout` gains a `breathStrip()` between the bar strip and the name area.
  - `Hud` draws hearts, bolts and bubbles as simple vector shapes, with half icons clipped to the left half. No new art is needed.

### Not in scope
- Food and hunger (`animals`).
- Enemy damage (`enemies`).
- Armour.
- Dropping items on death.
- Saving vitals.
- A per-player respawn point or bed.
- Game difficulty settings.

## Tech Stack / Commands

Unchanged: C++20, `make`, `make test`. No new dependencies.

## Project Structure

```
inc/vitals/vitals.hpp, src/vitals/vitals.cpp        → Vitals, FallTracker (pure)
inc/sim/protocol.hpp                                 → MovePlayer flags; VitalsChanged; Respawned
inc/sim/player_state.hpp, inc/sim/game.hpp, src/sim/game.cpp → integration, death/respawn
inc/player/player.hpp, src/player/player.cpp        → setSprintAllowed, respawn reset
inc/ui/hud_layout.hpp, src/ui/hud_layout.cpp        → breathStrip
inc/ui/hud.hpp, src/ui/hud.cpp                      → hearts, bolts, bubbles, flash, death overlay
src/core/engine.cpp, inc/core/engine.hpp            → wiring
tests/vitals_test.cpp, tests/game_test.cpp, tests/hud_layout_test.cpp
```

## Testing Strategy

- **`Vitals`:**
  - Sprint drain and recovery timing.
  - Exhaustion and the threshold of 6 before sprinting again.
  - Breath drain and refill.
  - Drowning damage cadence.
  - The regen delay, including that damage resets it.
  - Clamping at 0 and 20, death at 0, and `reset`.
  - Fly mode doesn't drain stamina.
- **`FallTracker`:**
  - Falls of 3 blocks or less do no damage, and 4 blocks does 1.
  - Damage is measured from the peak after a jump, not the take-off height.
  - Water landings and fly mode do no damage, and fly mode resets the fall.
  - Several landings in a row each count separately.
- **`Game`:**
  - A reported fall damages the owner, who gets `VitalsChanged`.
  - Drowning over ticks.
  - Regen over ticks.
  - At 0 health the player is dead: their commands are ignored, and after 3 s they get `Respawned` at spawn with full vitals and an unchanged inventory.
  - Other players never get your vitals.
- **`HudLayout`:** the breath strip sits between the bar strip and the name area, inside the window.
- **Manual check:**
  - Jump off something tall and lose hearts.
  - Stay underwater: bubbles go down, then hearts.
  - Sprint until you can't, then recover.
  - Die and respawn with your items.
  - Watch hearts regenerate.

## Boundaries

- **Always:**
  - Vitals change only in `Game`.
  - The pure classes have no `Game`/`World` dependency.
  - Run `make test`.
- **Ask first:**
  - Changing any rate, threshold or the respawn rule.
  - Saving vitals.
  - Adding difficulty settings.
- **Never:**
  - Let the client decide damage or health.
  - Drop or delete items on death.
  - Change the save format.

## Success Criteria

1. Every acceptance criterion is covered by a passing test, except the drawing.
2. The manual check passes.
3. The build has zero new warnings and CI is green.

## Open Questions

None. Decisions so far: hearts and bolts as icons; keep your inventory on
death; damage from falling and drowning; slow natural regen.
