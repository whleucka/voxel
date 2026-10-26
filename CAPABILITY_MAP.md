# Capability Map: Voxel → playable game

Turning the engine prototype into a small survival game the family can play
together on the home network. Each module gets its own `SPEC-<module-id>.md`
next to this file; this map is the index of what exists.

| Module id | Responsibility | Depends on |
|---|---|---|
| sim-core | Split authoritative game state from rendering; input becomes commands; world supports N players | — |
| items | Item definitions (blocks + non-block items), ItemStack, stack limits | — |
| vitals | Health, stamina (sprint drain/regen), damage, death & respawn | sim-core |
| inventory | Classic grid: 10-slot hotbar + storage grid, move/split stacks, survival break→collect / place→consume, persisted | items, sim-core |
| hud | Hotbar (keys 1..0) with icons + counts, health/stamina bars, inventory screen | inventory, vitals |
| entities | Generic non-player entities: AABB physics, box-model rendering, spawn/despawn near players | sim-core |
| animals | Passive mobs: biome spawning, wander AI, drops | entities, items |
| enemies | Hostile mobs: spawn rules, voxel pathfinding, melee attack | entities, vitals |
| multiplayer | LAN host/join, sync edits, players, entities, inventories | sim-core (+ all above) |

Build order: sim-core → items → vitals, inventory → hud → entities → animals → enemies → multiplayer

## Decisions

- **Game mode:** survival. Blocks are collected by breaking and consumed by placing.
- **Platforms:** Linux only.
- **Inventory:** classic grid.
- **Multiplayer:** home LAN only, no accounts. One player hosts in-process.
- **Out of scope:** crafting, internet play, a dedicated headless server.
- **Assets:** original only. The game is in the voxel-survival genre but does not copy any specific game.
