# Spec: multiplayer

Module of [CAPABILITY_MAP.md](CAPABILITY_MAP.md). Depends on: every module so far (`sim-core` … `enemies`).

## Objective

Let the kids play in the same world on the home network. One computer
**hosts**: it runs the authoritative `Game` in-process, as single-player
does today, and also accepts guests. The others **join** from a title
menu that finds games on the network by itself, so nobody types an IP
address. Each player has a name. Guests keep their inventory and position
in the host's save, so they carry on where they left off. Play is
cooperative: players can't hurt each other, and only the host can use the
F1 cheats.

Decisions already made in the capability map: home LAN only, no accounts,
the host plays in the same process, no internet play, no dedicated server.

### Acceptance criteria

**Title menu** (shown at launch)
- **Name field:** filled in from `player-name` in `voxel.properties`. It's saved back when changed. Names are 1–16 characters: letters, digits, `_` and `-`.
- **Single player:** today's game. Nothing is opened on the network.
- **Host game:** the same world, also listening for guests. The F1 panel shows the host's LAN address and the number of players.
- **Join:**
  - Lists the games found on the network as *world name, host name, players*, refreshed live.
  - Clicking one joins it.
  - An "address" box is the fallback for when discovery is blocked.
- **Quit.**
- **Errors:** a failed or lost connection returns to the menu with a one-line reason, such as "Host left the game", "Name already in use", "Game is full" or "Different game version".

**Network**
- **Transport:** TCP, port `server-port` (default 47800), with Nagle off.
  - Every message is a length-prefixed binary frame.
  - The first frames are a handshake (magic + protocol version). A version mismatch is refused with a clear reason.
- **Discovery:**
  - The host broadcasts a small UDP beacon every second on port 47801: magic, version, world name, host name, player count and TCP port.
  - The join list drops a game after 3 s without a beacon.
- **Sockets:** everything is non-blocking and polled on the main thread each frame. There are no new threads.
- **Bad input:**
  - A malformed or oversized frame (> 4 MB) drops that one connection. It never crashes the host.
  - Unknown command or event tags are treated as malformed.
- **Players:** at most 8, including the host.

**Joining and leaving**
- **Join:** `Join` gains `name`. The host is always the first `Join`, from its own in-process link.
- **Guest records:**
  - A guest whose name has a record in the save gets back their position, look direction, hotbar slot, fly mode and inventory.
  - A new name starts at the world spawn with the starter kit.
  - A name already in the game is rejected.
- **What a guest gets on joining:** a `Welcome` with the seed, noise offset, time of day, time scale and **all player edits so far**. Their client generates the same terrain and replays the edits, so the world matches the host's.
- **Leaving:** when a guest leaves or drops, their record is written to the save at once, and everyone gets `PlayerLeft`.
- **Host quits:** everyone is saved, guests are told "Host left the game" and return to the menu.

**Playing together**
- **Block edits:** every edit reaches every client. A guest's client records edits for chunks it hasn't loaded and applies them when the chunk loads.
- **Other players:**
  - Each player sees the others as a blocky figure in that player's colour, with swinging legs while moving and a **name tag** above the head.
  - They're drawn smoothly, between snapshots, like entities.
  - The colour comes from the name, out of 8.
- **The clock:** the host sends a `TimeSync` every second. Guests run their own clock between syncs, so the sky matches.
- **The rest already works per player** through the existing events: inventory, vitals, death and respawn, entities, feeding, eating, dropping and picking up items, and enemy attacks.
- **No PvP:** players can't hit each other. The aim ray only targets entities, and players aren't entities.
- **Cheats:** `GiveItem`, `SetTimeOfDay`, `SpawnDebugEntity` and `Save` from a guest are rejected ("host only"). The guest's F1 panel shows information only. The difficulty is the host's setting.
- **Stale moves after a respawn** (the known issue): `Respawned` carries a respawn counter, `MovePlayer` echoes it, and `Game` ignores moves with an old counter.

**Saving**
- **Host save** (F5 or quit) writes the world as today, plus `players/<name>.dat` for every guest who is in the game or has played.
  - Each file holds position, look direction, slot, fly mode and inventory.
- **The host's own record stays in `player.dat` and `inventory.dat`.** Existing saves load unchanged, and a save without `players/` just has no guests yet.

## Design

```
 Host process                                        Guest process
 ─────────────────────────────────────────────       ─────────────────────────────
 Engine ── LocalConnection ──┐                       Engine
                             ▼                         │ own World (seed + edits),
 Game ◄── NetServer ◄─TCP frames─► NetConnection ◄─────┘ own GameClock (TimeSync)
   │       (one RemoteClient per guest: a ClientLink)
   └─ Discovery beacon (UDP broadcast) ─────────►  LanBrowser (join list)
```

```cpp
// net/wire.hpp: pure, unit-tested. One encode/decode per Command and Event
// alternative; a missing one is a compile error (std::visit, as Game::apply).
std::vector<uint8_t> encode(const Command &);  std::optional<Command> decodeCommand(std::span<const uint8_t>);
std::vector<uint8_t> encode(const Event &);    std::optional<Event>   decodeEvent(std::span<const uint8_t>);
class FrameReader { void feed(std::span<const uint8_t>); std::optional<std::vector<uint8_t>> next(); bool bad() const; };

// net/socket.hpp: thin RAII wrappers over POSIX non-blocking sockets.
// net/net_server.hpp:
class NetServer { bool listen(uint16_t port); void poll(Game &); /* accept, read, flush */ };
// net/net_connection.hpp: a Connection over TCP, plus connect()/status()/error().
// net/discovery.hpp: Beacon {world, host, players, max, port}; encode/decode (pure);
//                    BeaconSender (host); LanBrowser (join list with 3 s expiry).

// protocol additions
struct Join      { std::string name; };                 // was empty
struct MovePlayer{ …; uint32_t respawn = 0; };
struct Welcome   { uint32_t seed; glm::vec2 noise_offset; float time_of_day, time_scale;
                   WorldEdits edits; };                 // guests only, before Joined
struct TimeSync  { float time_of_day; float time_scale; };
struct Respawned { glm::vec3 position; uint32_t respawn; };
struct PlayerView{ PlayerId id; std::string name; glm::vec3 position; float yaw, pitch;
                   bool crouch; };                      // in EntitySnapshot::players (others only)
struct Disconnected { std::string reason; };            // client-side only, from NetConnection
```

- **Engine refactor:**
  - `Engine` owns `std::unique_ptr<Game>` (single-player and host only) and a `World &` it renders and collides against. That's the `Game`'s world on a host, or its own on a guest.
  - Guest-only parts: the client world and clock, `BlockChanged` handling, and `Welcome`.
  - The menu is an ImGui screen drawn before any world exists.
- **`Game` additions:**
  - `addClient(link, host)`, `removeClient(link)`, `isHost(client)`.
  - Name validation and the player cap.
  - The guest-record store, loaded from `players/` and written on leave and on save.
  - `Welcome` and `TimeSync`, plus `players` in snapshots.
- **`World` addition:** `applyEdit(pos, type)`. It records the edit for unloaded chunks, so they replay on load, and sets the block for loaded ones.
- **Guest movement:** still client-authoritative, as today. That's fine on a home LAN, and it keeps movement smooth.

### Not in scope
- Internet play, NAT and port forwarding.
- Accounts and passwords.
- Chat. *(Easy to add later; ask.)*
- A dedicated or headless server.
- Encryption.
- Lag compensation for hits.
- Spectators.
- Changing the host's save format beyond adding `players/`.

## Tech Stack / Commands

Unchanged: C++20, `make`, `make test`. Sockets are POSIX (`<sys/socket.h>`), since the game is Linux only; there are no new dependencies. New properties keys: `player-name` (default `Player`) and `server-port` (default 47800).

## Project Structure

```
inc/net/wire.hpp, src/net/wire.cpp                     → frames + codecs (pure)
inc/net/socket.hpp, src/net/socket.cpp                 → RAII POSIX sockets
inc/net/net_server.hpp, src/net/net_server.cpp         → host side
inc/net/net_connection.hpp, src/net/net_connection.cpp → guest side
inc/net/discovery.hpp, src/net/discovery.cpp           → beacon, LAN browser
inc/sim/protocol.hpp, inc/sim/game.hpp, src/sim/game.cpp → names, host-only, records, Welcome, TimeSync, players
inc/world/world.hpp, src/world/world.cpp, world_save.* → applyEdit; players/<name>.dat
inc/ui/title_menu.hpp, src/ui/title_menu.cpp           → menu
inc/render/player_renderer.* (or entity_renderer)      → other players + name tags
src/core/engine.cpp, inc/core/engine.hpp, src/core/config.cpp, src/main.cpp
tests/wire_test.cpp, tests/net_test.cpp, tests/discovery_test.cpp, tests/game_test.cpp, tests/world_save_test.cpp, …
```

## Testing Strategy

- **Wire format:**
  - Every `Command` and `Event` alternative round-trips, including edge values (negative coordinates, empty and full inventories, big edit lists, long names).
  - Truncated, oversized and garbage frames are rejected without crashing.
  - Frames fed one byte at a time reassemble.
  - A version mismatch is refused.
- **Loopback TCP** (127.0.0.1, ephemeral port, in-process host `Game`):
  - A guest joins and gets `Welcome` then `Joined`.
  - Its `BreakBlock` reaches the host's world, and both clients get `BlockChanged`.
  - Snapshots list the other player.
  - A disconnect gives `PlayerLeft`, and the guest's record is saved.
  - A duplicate name, a full game, and a host quitting each end with the right reason.
- **Discovery:**
  - The beacon round-trips.
  - The browser adds a game on a beacon and expires it after 3 s, with time injected.
  - Loopback unicast sending works. Broadcast itself is a manual check.
- **`Game`:**
  - Names are validated.
  - A guest record is restored on rejoin, and a new name gets the starter kit.
  - Host-only commands are rejected for guests.
  - Moves with an old respawn counter are ignored.
  - Each client's snapshot excludes its own player.
  - `TimeSync` arrives once a second.
- **`World::applyEdit`:**
  - An edit to an unloaded chunk appears when the chunk loads.
  - A guest world built from `Welcome` matches the host's blocks.
- **Saves:**
  - `players/<name>.dat` round-trips.
  - Old saves without `players/` load.
- **Manual check:** two computers on the home network, or two copies on one machine with different names.
  - The guest finds the host in the list and joins.
  - Both see each other with name tags.
  - Building, breaking, mobs, items and the day/night cycle all agree on both screens.
  - The guest leaves and rejoins with their stuff.
  - The host quits, and the guest returns to the menu.

## Boundaries

- **Always:**
  - The `Game` stays authoritative for everything except movement.
  - Validate every decoded message. A bad guest can only drop itself.
  - Run `make test`.
  - Single-player must keep working without touching the network.
- **Ask first:**
  - Adding chat or PvP.
  - Playing over the internet.
  - Changing the ports.
  - Raising the player cap.
  - Any save format change beyond `players/`.
  - Adding a networking library.
- **Never:**
  - Let a guest run cheat commands.
  - Let a malformed message crash the host.
  - Open the network in single-player.
  - Break loading of existing saves.

## Success Criteria

1. Every rule above is covered by passing tests, apart from the drawing and broadcast delivery.
2. The manual two-player check passes.
3. The build has zero new warnings and CI is green.

## Open Questions

None. Decisions so far: a title menu with LAN discovery; guests kept per
name in the host's save; F1 cheats for the host only; no PvP.
