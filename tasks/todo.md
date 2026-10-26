# Tasks: sim-core

Plan: [plan.md](plan.md) · Spec: [SPEC-sim-core.md](../SPEC-sim-core.md)

**Every task's definition of done:**
- `make -j"$(nproc)"` builds with no new warnings.
- `make test` passes.
- The game launches and plays.
- One commit per task.

---

## Task 1: Test harness (doctest + `make test` + CI)

**Description:** Vendor doctest and add a `test` target. The target builds `bin/tests` from `tests/*.cpp` plus every app object except `obj/main.o`, links the same libs, and runs it. Add a smoke test and a CI step.

**Acceptance criteria:**
- [x] `external/doctest/doctest.h` is vendored, with its version noted in a header comment.
- [x] `make test` builds and runs `bin/tests` and exits non-zero on failure.
- [x] CI runs `make test` after the build step.

**Verification:**
- [x] `make test` passes locally with 1 smoke test.
- [x] Temporarily break the smoke test and confirm `make test` fails.
- [x] `make` still produces `bin/app`.

**Dependencies:** None

**Files likely touched:** `external/doctest/doctest.h`, `Makefile`, `tests/main_test.cpp`, `.github/workflows/build.yml`

**Estimated scope:** Small

---

## Task 2: Characterization tests for World edits and save

**Description:** Pin down today's behaviour before refactoring. Add `World::loadChunkSync(cx, cz)`, which generates a chunk on the calling thread with no meshing queue and no GL, for tests. Write tests for the current `setBlockAt`/`getBlockAt`/edits behaviour and for the `WorldSave` round-trip.

**Acceptance criteria:**
- [x] With a fixed seed and a sync-loaded chunk, `setBlockAt` then `getBlockAt` returns the new type, and `snapshotEdits()` contains the delta (AIR when a block is broken).
- [x] `WorldSave::save` followed by `load*` into a temp dir round-trips `LevelData`, `PlayerData` and `WorldEdits` exactly.
- [x] `setBlockAt` outside the 0..kChunkHeight range, or into an unloaded chunk, is a no-op.

**Verification:**
- [x] `make test` is green, with the new tests listed.
- [x] The tests construct no window and make no GL calls. They pass in CI on a machine with no display.

**Dependencies:** T1

**Files likely touched:** `inc/world/world.hpp`, `src/world/world.cpp`, `tests/world_test.cpp`, `tests/world_save_test.cpp`

**Estimated scope:** Small–Medium

---

## Checkpoint A
- [ ] `make test` passes locally and in CI.
- [ ] The game plays exactly as before.

---

## Task 3: World no longer owns Player

**Description:**
- Move `std::unique_ptr<Player>` from `World` to `Engine`.
- Extract `World::findSpawn()` from `init()`.
- Replace player-driven chunk streaming with `World::setInterestPoints(std::span<const glm::vec3>)`.
- Change `World::render` to take the frustum planes, eye position and underwater flag.
- `Player::update(dt, World*)` stays unchanged. Engine calls it.
- The loaded `PlayerData` is applied by Engine, not World.

**Acceptance criteria:**
- [x] `World` has no `player` member and no `getPlayer()`. `grep -n "player" src/world/world.cpp` shows only comments or parameters.
- [x] Chunks stream around the point set by `setInterestPoints`. A unit test with 2 points far apart loads chunks around both.
- [x] A saved player's position, look direction, slot and fly mode are still restored.

**Verification:**
- [x] `make test` passes, including the new test for multiple interest points.
- [ ] Manual check on a fresh world and an existing save:
  - Spawn point is correct.
  - Chunks stream in as you walk.
  - Shadows and clouds follow the player.
  - Underwater fog still works.

**Dependencies:** T2

**Files likely touched:** `inc/world/world.hpp`, `src/world/world.cpp`, `inc/core/engine.hpp`, `src/core/engine.cpp`, `tests/world_test.cpp`

**Estimated scope:** Medium

---

## Task 4: Protocol, Connection, LocalConnection, Game skeleton

**Description:**
- Add `inc/sim/protocol.hpp` with the `Command` and `Event` `std::variant`s for the full protocol table in the spec.
- Add a `Connection` interface (`send(Command)`, `poll(std::vector<Event>&)`) and `LocalConnection`, a pair of in-process FIFO queues.
- Add `Game`, which owns `World`, `GameClock` and `std::unordered_map<PlayerId, PlayerState>`. It also has `pump()` (drain commands), `tick()` (fixed 50 ms) and an accumulator helper `advance(dt)`.
- Implement `Join`: it creates a `PlayerState` at `findSpawn()` or at the saved player and replies with `Joined`.
- Engine owns `Game` and the connection. It sends `Join` at startup and initializes its `Player` from `Joined`.

**Acceptance criteria:**
- [x] The clock advances only in `Game::tick()`. `advance(0.125f)` runs exactly 2 ticks and carries the remainder.
- [x] `Join` returns `Joined` with a spawn position on solid ground.
- [x] Engine no longer owns `World` or `GameClock` directly.

**Verification:**
- [x] `make test` passes with the new `game_test.cpp`: the tick accumulator, `Join` on a fresh world, and `Join` with loaded `PlayerData`.
- [ ] Manual check: the day/night cycle runs at the same speed (time scale slider in F1 still works), and spawn is unchanged.

**Dependencies:** T3

**Files likely touched:** `inc/sim/protocol.hpp`, `inc/sim/connection.hpp`, `inc/sim/local_connection.hpp` (+ `.cpp`), `inc/sim/game.hpp`, `src/sim/game.cpp`, `src/core/engine.cpp` (+ `.hpp`), `tests/game_test.cpp`

**Estimated scope:** Medium. This is the largest task. If it runs long, split it into 4a (protocol + LocalConnection + tests) and 4b (Game + Engine wiring).

---

## Checkpoint B
- [ ] All tests pass and CI is green.
- [ ] Full manual parity checklist (spec criterion 5) passes.
- [ ] **Human review of the new `sim/` structure before routing gameplay through it.**

---

## Task 5: MovePlayer and SelectSlot commands

**Description:**
- Engine sends `MovePlayer {pos, yaw, pitch, fly, crouch}` each frame after local physics.
- `Game` stores it in `PlayerState` and feeds every player's position into `World::setInterestPoints`.
- Number keys 1–9 and the scroll wheel send `SelectSlot`. `Game` clamps it to `0..kHotbarSlots-1` and stores it.

**Acceptance criteria:**
- [x] Chunk streaming is driven by `PlayerState` positions, not by Engine directly.
- [x] `SelectSlot {42}` is clamped or rejected, and the state stays valid.
- [ ] Movement and slot selection feel identical to before.

**Verification:**
- [x] `make test` passes: `MovePlayer` updates the state and the interest points; `SelectSlot` is clamped.
- [ ] Manual check: walk, sprint, fly (T), scroll and number keys.

**Dependencies:** T4

**Files likely touched:** `inc/sim/player_state.hpp`, `src/sim/game.cpp`, `src/core/engine.cpp`, `tests/game_test.cpp`

**Estimated scope:** Small

---

## Task 6: BreakBlock and PlaceBlock validated in Game

**Description:**
- Move all validation from `Engine::mouseButtonCallback` into `Game::apply`. That covers reach from the player's eye, bedrock/water, the Y range, the target cell being air or liquid, and not overlapping *any* player's box (using the crouch height).
- A valid edit calls `World::setBlockAt` and broadcasts `BlockChanged`. An invalid one sends `CommandRejected` (shown only in the debug panel).
- Engine sends commands only.

**Acceptance criteria:**
- [x] `grep -n "setBlockAt" src/core` returns nothing.
- [x] Each rejection rule has a test, and a valid break or place emits exactly one `BlockChanged`.
- [x] Glowstone placement still lights the area, and breaking it removes the light.

**Verification:**
- [x] `make test` passes with the validation tests.
- [ ] Manual check:
  - Break and place at max reach.
  - Try to place inside yourself, standing and crouched.
  - Place and break glowstone.
  - Try bedrock and water.

**Dependencies:** T5

**Files likely touched:** `src/sim/game.cpp`, `inc/sim/game.hpp`, `src/core/engine.cpp`, `tests/game_test.cpp`

**Estimated scope:** Small–Medium

---

## Task 7: Game owns save/load

**Description:**
- Move loading out of `Engine::run()`: `Game` is constructed from a save dir and loads level, edits and player data.
- Move `saveGame()` into `Game::save()`.
- F5 and the ImGui button send `Save {}`. Quitting calls `game.save()` directly.
- The save format is unchanged.

**Acceptance criteria:**
- [x] Saves written before this change load correctly. Use a copy of a real `saves/` world.
- [x] A test plays commands into a `Game` (break, place, select slot, move), saves to a temp dir, and constructs a new `Game` from it. Edits and player state match.
- [x] Engine contains no `WorldSave::` calls. *(Only the `WorldSave::worldDir()` path helper remains; it does no file I/O.)*

**Verification:**
- [x] `make test` passes with the round-trip test.
- [ ] Manual check: edit, press F5, quit, relaunch. Edits and position persist. Load an old save.

**Dependencies:** T4 (T5/T6 recommended first so the test can use real commands)

**Files likely touched:** `inc/sim/game.hpp`, `src/sim/game.cpp`, `src/core/engine.cpp`, `inc/core/engine.hpp`, `tests/game_test.cpp`

**Estimated scope:** Small–Medium

---

## Checkpoint C / Task 8: Parity, performance and cleanup

**Description:** Check every spec success criterion, measure FPS against `main`, and update the README features and structure and the spec's status.

**Acceptance criteria:**
- [ ] Spec success criteria 1–7 are all checked.
- [ ] FPS at spawn (debug panel, default settings, same seed) is within 5% of `main`. Both numbers are recorded in the PR.
- [ ] The build has zero new warnings and CI is green.

**Verification:**
- [x] `make test` passes, and `make DEBUG=1` builds clean.
- [ ] The full manual parity checklist passes on a fresh world and an existing save.
- [ ] **Human review and approval of the PR.**

**Dependencies:** T1–T7

**Files likely touched:** `README.md`, `SPEC-sim-core.md`, `tasks/todo.md`

**Estimated scope:** Small
