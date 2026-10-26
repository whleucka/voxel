# Capability Map: Voxel → playable game

Turning the engine prototype into a small survival game the family can play
together on the home network. Each module gets its own `SPEC-<module-id>.md`
next to this file; this map is the index of what exists.

| Module id | Responsibility | Depends on |
|---|---|---|
| sim-core ✅ | Split authoritative game state from rendering; input becomes commands; world supports N players | — |
| items ✅ | Item definitions (blocks + non-block items), ItemStack, stack limits | — |
| vitals | Health, stamina (sprint drain/regen), damage, death & respawn; adds HUD bars | sim-core, hud |
| inventory ✅ | Classic grid: 10-slot hotbar + storage grid, move/split stacks, survival break→collect / place→consume, persisted | items, sim-core |
| hud ✅ | Hotbar (keys 1..0) with icons + counts, inventory screen; reserves space for vitals bars | inventory, items |
| entities | Generic non-player entities: AABB physics, box-model rendering, spawn/despawn near players | sim-core |
| animals | Passive mobs: biome spawning, wander AI, drops | entities, items |
| enemies | Hostile mobs: spawn rules, voxel pathfinding, melee attack | entities, vitals |
| multiplayer | LAN host/join, sync edits, players, entities, inventories | sim-core (+ all above) |

Build order: sim-core → items → inventory → hud → vitals → entities → animals → enemies → multiplayer
(hud moved ahead of vitals on 2026-10-26, so the hotbar is visible sooner)

## Decisions

- **Game mode:** survival. Blocks are collected by breaking and consumed by placing.
- **Platforms:** Linux only.
- **Inventory:** classic grid.
- **Multiplayer:** home LAN only, no accounts. One player hosts in-process.
- **Out of scope:** crafting, internet play, a dedicated headless server.
- **Assets:** original only. The game is in the voxel-survival genre but does not copy any specific game.

## Known issues found along the way

- ~~**World generation is not deterministic**~~ (found in sim-core T6; fixed
  in `fix/seeded-worldgen`). Trees, gems and bedrock used the global `rand()`
  from the generation worker threads, so a chunk's contents depended on thread
  timing. Generation now draws from a per-chunk RNG seeded from
  `(world_seed, cx, cz)`, so a chunk is identical on every reload and every
  machine with the same seed. That was a prerequisite for `multiplayer`.
- **Stale moves after a respawn** (noted in vitals T3). `Game` trusts
  `MovePlayer` positions. In-process, a respawn is always applied before
  the client's next move, but over a network a move sent before `Respawned`
  could arrive after it. It would put the player back where they died and
  could count a fall. **For `multiplayer`:** tag moves with a respawn or
  teleport counter and drop stale ones.
