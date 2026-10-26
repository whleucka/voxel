# Tasks: title-screen

Plan: [title-screen-plan.md](title-screen-plan.md) · Spec: [SPEC-title-screen.md](../SPEC-title-screen.md)

**Every task's definition of done:**
- `make` builds with no warnings.
- `make test` passes.
- One commit per task.

## T1: Seeds
- [x] `resolveSeed` (blank random, digits as-is, text hashed) and `noiseOffsetFor` are shared, and the config tests pass.

## T2: World list
- [ ] Lists worlds newest first with seed, last played and size. Unreadable folders are skipped.
- [ ] Create, rename and delete work. They refuse invalid or taken names, missing worlds, and escapes from `saves/`.
- [ ] `lastPlayedText` covers minutes, today, yesterday and days ago.

## T3: Menu scene
- [ ] The orbit keeps its distance and height, looks at the centre, and repeats every period.
- [ ] `MenuScene` loads the last world (or a scenic seed), orbits, and runs its clock. It never saves.

## T4: Menu pages
- [ ] Main, Single player (world list and actions with a delete confirm) and Multiplayer pages.

## T5: Engine
- [ ] The scene shows on the menu and closes before a session. Play and Host start the chosen world and make it the last played. Rename follows `world-name`.
- [ ] Manual check (user): the spec's manual check.

## T6: Wrap-up
- [ ] Map and spec status are updated, CI is green, and the PR is merged.
