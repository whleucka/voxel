# Implementation Plan: title-screen

Spec: [SPEC-title-screen.md](../SPEC-title-screen.md) · Tasks: [title-screen-todo.md](title-screen-todo.md)

## Architecture Decisions
- **World management is pure file code over a root path.** That makes it
  testable against a temp dir, and keeps every path check (names, `..`,
  staying inside `saves/`) in one place.
- **The seed helpers move to `core/seed`.** `resolveSeed` and
  `noiseOffsetFor` are shared by the properties loader, `Engine::init` and
  world creation, so a new world gets exactly the seed the properties file
  would give it.
- **The menu scene is a plain `World` with no `Game`.** It loads the save's
  level and edits, streams around the orbit centre, and is rendered with the
  normal world renderer. The orbit maths is pure and tested.
- **The scene is closed before any session starts**, because world
  generation reads the global seed.

## Tasks
- [ ] T1: `core/seed`: `resolveSeed`, `noiseOffsetFor` (moved, tested)
- [ ] T2: World list: list, create, rename, delete, last-played text (tests)
- [ ] T3: Menu scene: orbit maths (tests) and `MenuScene` (GL)
- [ ] T4: Title menu pages and the world list UI
- [ ] T5: `Engine` wiring: scene lifecycle, Play/Host the chosen world, last played
- [ ] T6: Wrap-up, PR, merge when green

## Risks and Mitigations
| Risk | Impact | Mitigation |
|---|---|---|
| Deleting the wrong folder | High | One validated name, resolved and checked to be a direct child of `saves/`. Confirm step. Tests for escapes. |
| The scene world's threads racing a new session's seed | Med | Close the scene (which joins its threads) before setting the seed or starting a session. |
| Menu FPS | Low | Same renderer as the game. The scene streams like normal play. |
