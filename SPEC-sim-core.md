# Spec: sim-core

Module of [CAPABILITY_MAP.md](CAPABILITY_MAP.md). Depends on: nothing.

## Objective

Separate *what the game state is* from *how one machine shows and controls
it*, so every later module (vitals, inventory, entities, multiplayer) is built
once against an authoritative simulation instead of against `Engine`'s GLFW
callbacks and `World`'s single `Player`.

This is a refactor. Nothing should look or play differently when it lands.

**User-visible outcome:** none. **Developer outcome:**
- Game state lives in `Game`: the world's block data and edits, the clock, and per-player state.
- `Game` changes state only by applying a `Command` from a client. It reports changes as `Event`s.
- `Engine` becomes the client. It turns input into commands, applies events, and renders.
- A `Connection` interface carries commands and events. Only an in-process `LocalConnection` exists for now. `multiplayer` later adds a socket implementation without touching game logic.
- `World` no longer owns a `Player`. It streams chunks around a set of interest points, one per connected player.

## Design

```
 Engine (client)                         Game (sim, authoritative)
 ─────────────────                       ──────────────────────────
 GLFW input ─► LocalPlayer (physics,     PlayerState map<PlayerId,…>
               camera, predicted move)   World (blocks, edits, light,
      │                                         chunk streaming)
      │ Command ──► Connection ──────────►  apply(Command) → validate
      │                                     tick(50 ms fixed)
      ◄──────── Event ◄── Connection ◄──── BlockChanged, PlayerJoined…
 Renderer ◄── reads World (shared in-process; replica in multiplayer)
```

- **Movement trusts the client.** `LocalPlayer` runs physics and collision on the client, as now, and sends `MovePlayer {pos, yaw, pitch, flags}` every frame. On a home LAN there's no cheating to defend against, and this keeps movement smooth with no prediction or rollback code.
- **The simulation owns everything else.** Block edits, the selected slot (later: inventory, health, entities) are validated and applied only inside `Game::apply`. That means the place/break checks move out of `Engine::mouseButtonCallback`: reach, bedrock/water, the height range, and not placing inside *any* player's box.
- **Fixed tick.** `Game::tick()` runs every 50 ms (20 Hz), driven by an accumulator in the client loop. The clock advances per tick. Rendering stays at frame rate.
- **The client reads the `World` directly.** In single-player the client renders the `Game`'s `World` and reads it for collision through a const reference. In multiplayer each client will hold a replica `World` generated from the seed and updated by `BlockChanged` events.
- **The renderer stays inside `World` for now.** Moving meshes and `Renderer` out of `Chunk`/`World` is only needed for a headless server, which is out of scope.

### Initial protocol

| Command (client→sim) | Event (sim→client) |
|---|---|
| `Join {}` → reply `Joined {player_id, spawn, yaw, pitch, slot, fly}` | `BlockChanged {pos, type}` |
| `MovePlayer {pos, yaw, pitch, fly, crouch}` | `PlayerJoined / PlayerLeft {id}` (unused until multiplayer) |
| `BreakBlock {pos}` | `CommandRejected {reason}` (debug only) |
| `PlaceBlock {pos, type}` (type comes from the hotbar until `inventory`) | |
| `SelectSlot {slot}` | |
| `Save {}` (F5) | |
| `SetTimeOfDay {seconds}` (debug panel slider; added in T4 so the panel doesn't write the clock directly) | |

Commands and events are plain structs in a `std::variant` and have no
serialization yet. `multiplayer` adds a wire format.

## Tech Stack

C++20, g++, GNU Make, OpenGL 3.3 core, GLFW, GLAD, GLM, Dear ImGui, stb,
robin_hood. **New (approval needed):** [doctest](https://github.com/doctest/doctest),
a single header vendored at `external/doctest/doctest.h`, for unit tests.

## Commands

```
Build:        make -j"$(nproc)"
Debug build:  make DEBUG=1 -j"$(nproc)"
Run:          ./bin/app            # from repo root; reads voxel.properties, saves/
Test (new):   make test            # builds bin/tests and runs it
Clean:        make clean
```

## Project Structure

```
inc/sim/, src/sim/        → NEW: game.hpp/cpp (Game), protocol.hpp (Command/Event
                            variants), player_state.hpp, connection.hpp,
                            local_connection.hpp/cpp
inc/player/, src/player/  → Player becomes the client-side LocalPlayer controller
inc/core/, src/core/      → Engine = client: window, input→commands, render, ImGui
inc/world/, src/world/    → World loses `player`; streams around interest points;
                            save/load called from Game
tests/                    → NEW: *_test.cpp, doctest; linked against src/ minus main.cpp
```

## Code Style

Match the existing code: `#pragma once`, 2-space indent, PascalCase types,
camelCase methods, snake_case members, `kPascalCase` constants, `// ─── Section ───`
banners in large .cpp files, and comments that explain *why*.

```cpp
// ─── Commands ──────────────────────────────────────────────────────────

void Game::apply(PlayerId id, const BreakBlock &cmd) {
  PlayerState *ps = findPlayer(id);
  if (!ps) return;

  // Reach is checked server-side so a remote client can't edit across the map.
  if (glm::distance(ps->eyePosition(), glm::vec3(cmd.pos) + 0.5f) > kPlayerReach)
    return reject(id, "out of reach");

  BlockType t = world.getBlockAt(glm::vec3(cmd.pos) + 0.5f);
  if (t == BlockType::BEDROCK || t == BlockType::WATER) return;

  world.setBlockAt(cmd.pos, BlockType::AIR);
  broadcast(BlockChanged{cmd.pos, BlockType::AIR});
}
```

## Testing Strategy

- **Unit tests (doctest, `tests/`):** command validation in `Game`, tested without a window.
  - Breaking bedrock or water is rejected.
  - Placing a block out of reach, outside 0..kChunkHeight, or inside a player's box is rejected.
  - A valid break or place produces exactly one `BlockChanged`.
  - `SelectSlot` is clamped.
  - The tick accumulator gives N ticks for N×50 ms.
  - Edits round-trip through save/load.
- **Window-free tests:** the tests construct `Game` and `World` with a small fixed set of chunks and no GL calls. `World` methods that upload to the GPU must not run in tests, so any GL use is reached only through the render path.
- **Manual parity check:** follow the checklist under Success Criteria on a fresh world and on a pre-existing save.
- **CI:** add a `make test` step to `.github/workflows/build.yml` (approval needed).

## Boundaries

- **Always:**
  - Keep behaviour identical.
  - Route every state change through `Game::apply`.
  - Build with no new warnings.
  - Run `make test` and the manual parity check before each commit.
  - Make small commits, one per task.
- **Ask first:**
  - Changing the save file format (`level.dat`, `player.dat`, `edits.dat`).
  - Adding dependencies other than doctest.
  - Changing controls or keybindings.
  - Moving rendering out of `World`/`Chunk`.
- **Never:**
  - Edit vendored code under `external/` (except adding doctest).
  - Break loading of existing `saves/`.
  - Let `Engine` call `World::setBlockAt` directly once the protocol exists.

## Success Criteria

1. `World` has no `Player` member or `getPlayer()`. Chunk streaming is driven by `World::setInterestPoints(std::span<const glm::vec3>)` or an equivalent.
2. `Engine` doesn't mutate `World` or game state directly. `grep -n "setBlockAt" src/core` returns nothing.
3. `Game::apply` handles every command in the protocol table, and all placement and break checks live there.
4. `make test` passes and covers the cases listed under Testing Strategy.
5. Manual parity on a fresh world and on an existing save:
   - Spawn position and look direction are restored.
   - WASD, jump, sprint, crouch and fly (T) feel unchanged.
   - Break and place work, including glowstone lighting and not placing inside yourself.
   - Number keys 1–9 and the scroll wheel select slots.
   - F5 and quitting both save, and the edits persist after a restart.
6. FPS on the default settings is within 5% of `main`, measured with the ImGui debug panel at spawn.
7. The build has zero new compiler warnings and CI is green.

## Open Questions

1. **doctest:** OK to vendor it and add `make test` plus a CI step?
2. **Tick rate:** I've assumed 20 Hz is fine for the clock now and for entities later. Any objection?
3. **Save format:** player.dat stays the local player's record for now. Multiplayer will need per-player saves keyed by name, so that change waits for the `multiplayer` spec. Agree?
