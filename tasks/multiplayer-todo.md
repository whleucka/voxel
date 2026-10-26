# Tasks: multiplayer

Plan: [multiplayer-plan.md](multiplayer-plan.md) · Spec: [SPEC-multiplayer.md](../SPEC-multiplayer.md)

**Every task's definition of done:**
- `make` builds with no warnings.
- `make test` passes.
- One commit per task.

## T1: Game: players
- [x] Names are validated. Duplicates and a full game are rejected with a reason.
- [x] `removeClient` gives `PlayerLeft`. Cheats from guests are rejected as host only.
- [x] Moves with an old respawn counter are ignored.

## T2: Game: shared world
- [x] Guests get `Welcome` (seed, offset, time, edits) before `Joined`. `TimeSync` arrives every second.
- [x] Snapshots list the other players, not yourself.
- [x] `World::applyEdit` takes effect when an unloaded chunk loads. A world built from `Welcome` matches the host's.

## T3: Guest records
- [x] `players/<name>.dat` round-trips. Old saves load.
- [x] Leaving saves the record, and rejoining restores it. A new name gets the starter kit.

## T4: Wire format
- [ ] Every Command and Event round-trips, including edge values.
- [ ] Frames reassemble from single bytes. Garbage, truncated and oversized frames are rejected.

## T5: Transport
- [ ] Loopback: join, edits seen by both, snapshots, leaving, and the right reasons for a duplicate name, a full game, a host quitting and a wrong version.

## T6: Discovery
- [ ] The beacon round-trips. The browser adds games and expires them after 3 s. Loopback send and receive works.

## T7: Engine refactor
- [ ] Single-player works through the optional `Game`. The guest mode uses its own `World` and clock.
- [ ] `player-name` and `server-port` round-trip.

## T8: Menu and flows
- [ ] The title menu offers Single player, Host, Join (live list and address) and Quit. Errors return to the menu with a reason.

## T9: Other players
- [ ] Blocky figures in name colours, with swinging legs and name tags, interpolated.
- [ ] Manual check (user): the spec's two-player check.

## T10: Wrap-up
- [x] Map and spec status are updated, CI is green, and the PR is merged.
