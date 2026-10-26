# Implementation Plan: multiplayer

Spec: [SPEC-multiplayer.md](../SPEC-multiplayer.md) · Tasks: [multiplayer-todo.md](multiplayer-todo.md)

## Architecture Decisions
- **The `Game` doesn't know about sockets.** Guests are just more
  `ClientLink`s. The `Game` gains names, a host flag per client,
  `removeClient`, guest records and the new events, all tested in-process
  with `LocalConnection`s before any networking exists.
- **The wire format is pure.** Codecs and framing are byte-in, byte-out, so
  they can be tested without sockets. Every variant alternative needs a
  codec, or the build fails (the same `std::visit` trick as `Game::apply`).
- **Sockets are non-blocking and polled on the main thread.** That means no
  locks and no new threads. The host polls `NetServer` before `Game::pump`.
  A guest polls its `NetConnection` inside `poll()`.
- **A guest's world is a normal `World`.** Its seed and edits come from
  `Welcome`. `World::applyEdit` records edits for unloaded chunks, so
  streaming applies them later.
- **The `Engine` refactor comes before any networking.** `Engine` owns an
  optional `Game` and renders through a `World &`. Single-player behaviour
  is pinned by the existing tests and a manual check.
- **The menu is ImGui**, drawn before a world exists. It's the only new UI.

## Tasks
- [x] T1: Protocol and `Game`: names, host flag, leaving, host-only cheats, respawn counter
- [x] T2: `Game`: `Welcome`, `TimeSync`, other players in snapshots; `World::applyEdit`
- [x] T3: Guest records in the save (`players/<name>.dat`)
- [x] T4: Wire format: frames and codecs for every Command and Event
- [x] T5: Sockets, `NetServer` and `NetConnection` (loopback tests)
- [x] T6: LAN discovery (beacon and browser)
- [ ] T7: `Engine` refactor: optional `Game`, guest world and clock, properties keys
- [ ] T8: Title menu, host and join flows, disconnect handling
- [ ] T9: Drawing other players with name tags
- [ ] T10: Wrap-up, PR, merge when green

### Checkpoints
- After T3: all `Game` rules are tested in-process.
- After T5: a guest can play over loopback in tests.
- After T8: two copies on one machine can play together (manual).

## Risks and Mitigations
| Risk | Impact | Mitigation |
|---|---|---|
| The `Engine` refactor breaks single-player | High | Do it as its own task. Keep the `LocalConnection` path, and keep the tests green. |
| Big `Welcome` messages (many edits) | Low | 5 bytes per edit. The 4 MB frame cap fits about 800k edits. |
| UDP broadcast blocked (firewall) | Med | The address box is a fallback. The host shows its address in F1. |
| Guest and host worlds drift | Med | Test: a world built from `Welcome` equals the host's blocks. |
| A slow guest backs up the host's send buffer | Low | Per-client outgoing buffer; drop the guest if it grows past 8 MB. |
