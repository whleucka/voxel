# Spec: title-screen

Module of [CAPABILITY_MAP.md](CAPABILITY_MAP.md). Depends on: `multiplayer` (title menu, `Session`).

**Status:** implemented. Criteria 1 and 3 are met; criterion 2 (manual check) is up to you.

## Objective

Make the title screen feel like the start of a game, and let the kids keep
more than one world. Behind the menu, the camera slowly circles a spot in
**the last world played**, live and with its sky, clouds and builds. A
**world list** lets them pick, create, rename and delete worlds. It shows
each world's details, and both Single player and Host game play from it.

### Acceptance criteria

**The scene behind the menu**
- **What it shows:** the last world played (`world-name` in `voxel.properties`). With no saves, it shows a fixed scenic seed.
- **The camera:**
  - It circles the world's saved player position (or its spawn) once every ~90 s.
  - It stays about 20 blocks out and 10 above, looking slightly down at the centre.
  - It flies **level**, at one height for the whole lap: at least 4 blocks above the highest ground anywhere on its circle. It looks gently down toward the centre, so it never climbs or dives with a slope. *(Changed after play-testing: the first version followed the ground under the camera, which was awkward on mountains.)*
- **Live but read-only:**
  - The world loads with its edits, and its clock runs at the normal day speed from the saved time.
  - There's no `Game`, no player, no creatures, and nothing is saved.
- **Fading in:** chunks stream in as usual and the scene fades in from the menu colour. The menu is usable straight away.
- **Hand-off:** the scene world is stopped before a game starts or a join begins, since world generation reads the global seed. It's rebuilt when you return to the menu.

**Menu pages**
- **Main:** the title, your name, and the buttons **Single player**, **Multiplayer** and **Quit**.
- **Single player:** the world list, plus **Play**, **Host on network**, **Create new world**, **Rename**, **Delete** and **Back**.
- **Multiplayer:** today's join list and address box, plus **Back**.
- **Feel:** the panels are semi-transparent over the scene. Messages such as "Host left the game" show on whichever page you're on.

**The world list** (folders under `saves/`)
- **Each row:** the world name, the seed, when it was last played (`level.dat`'s modified time, e.g. "today 14:05" or "3 days ago") and its size on disk.
- **Order:** most recently played first, and the last played world is selected when the list opens.
- **Names:** 1–32 characters of letters, digits, spaces, `_` or `-`. A name can't start or end with a space and can't be `.` or `..`. The name is the folder name.
- **Create:**
  - Enter a name and an optional seed. A blank seed is random, digits are used as they are, and other text is hashed, as in `voxel.properties`.
  - It's refused if the name is invalid or already taken.
  - It writes `level.dat` at once (seed, noise offset, noon), so the world is listed and loads with that seed.
- **Rename:** a new valid name that isn't taken. If it's the last played world, `world-name` follows the rename.
- **Delete:**
  - A confirm step reads "Delete 'castle'? This can't be undone."
  - It removes only that folder under `saves/`, and refuses anything that resolves outside it.
- **Play / Host:** start the selected world. That world becomes `world-name`, the last played world.
- **Guests:** joining someone else's world doesn't change your last played world.

## Design

```cpp
// world/world_list.hpp: pure file operations under a saves root (tests use
// a temp dir). Results say what went wrong for the menu to show.
struct WorldInfo { std::string name; uint32_t seed; std::filesystem::file_time_type
                   last_played; uintmax_t size_bytes; };
std::vector<WorldInfo> listWorlds(const std::filesystem::path &root);   // newest first
bool validWorldName(std::string_view);
std::optional<std::string> createWorld(root, name, seed_text);          // error text, or nothing
std::optional<std::string> renameWorld(root, from, to);
std::optional<std::string> deleteWorld(root, name);
std::string lastPlayedText(file_time_type, now);                        // "today 14:05", "3 days ago"

// core/seed.hpp: shared with config.cpp and Engine::init
uint32_t resolveSeed(std::string_view text); glm::vec2 noiseOffsetFor(uint32_t seed);

// render/menu_scene.hpp: a World plus an orbiting camera and a clock.
struct OrbitView { glm::vec3 eye, target; };
OrbitView orbitAt(glm::vec3 centre, double seconds);                    // pure
class MenuScene { bool open(const std::string &save_dir); void close();
                  void frame(float dt, int w, int h); };                // GL
```

- **The title menu** becomes three pages (main, single player, multiplayer). `TitleMenu::Choice` gains the chosen world name for Play and Host.
- **Engine:**
  - It keeps a `MenuScene` while no game is running.
  - On Play or Host it closes the scene, sets `world-name` and saves the properties, then starts the session for `saves/<name>`.
  - On Join it closes the scene, then joins.
- **Level flight:** twice a second, the scene samples the ground at 48 points on the camera's circle (in loaded chunks). The lap height only ever rises, to 4 blocks above the highest point found, and the camera eases toward it.

### Not in scope
- World thumbnails.
- Game modes per world.
- Copying worlds.
- Importing or exporting worlds.
- Music.
- A logo image (the title is text).
- Splash texts.

## Tech Stack / Commands

Unchanged: C++20, `make`, `make test`. There are no new dependencies and no new files in the save format.

## Project Structure

```
inc/world/world_list.hpp, src/world/world_list.cpp   → list/create/rename/delete (pure fs)
inc/core/seed.hpp, src/core/seed.cpp                 → resolveSeed, noiseOffsetFor (moved)
inc/render/menu_scene.hpp, src/render/menu_scene.cpp → orbit (pure) + scene (GL)
inc/ui/title_menu.hpp, src/ui/title_menu.cpp         → pages, world list UI
src/core/engine.cpp, src/core/config.cpp
tests/world_list_test.cpp, tests/menu_scene_test.cpp, tests/difficulty_test.cpp (seed)
```

## Testing Strategy

- **The world list**, against a temp `saves/`:
  - Lists folders newest first with seed and size. Folders without a readable `level.dat` are skipped.
  - Create writes a `level.dat` that `WorldSave::loadLevel` reads with the right seed. Blank seeds are random, digits are kept, text is hashed.
  - Create, rename and delete refuse invalid names, taken names, missing worlds, and paths that escape `saves/` (`..`, `/`).
  - Rename and delete do what they say, and only that.
  - `lastPlayedText` covers minutes, today, yesterday and days ago.
- **Seeds:** `resolveSeed` and `noiseOffsetFor` behave as before, and the config tests still pass.
- **The orbit:**
  - It stays at a constant distance from the centre, at the right height, and looks at the centre.
  - It comes back to the start after one period.
- **Manual check:**
  - The menu shows your last world circling slowly, with its sky.
  - Create, rename and delete work, with the confirm step.
  - Play and Host start the chosen world.
  - Returning to the menu shows that world.
  - FPS on the menu is fine.

## Boundaries

- **Always:**
  - Only touch folders directly under `saves/`.
  - Close the scene world before a session starts.
  - Run `make test`.
- **Ask first:**
  - Changing the save format.
  - Deleting more than the one chosen world.
  - Adding music or images.
- **Never:**
  - Delete without the confirm step.
  - Follow a path outside `saves/`.
  - Let the menu scene write to a save.

## Success Criteria

1. Every rule above is covered by passing tests, apart from the drawing.
2. The manual check passes.
3. The build has zero new warnings and CI is green.

## Open Questions

None. Decisions so far: a live world slowly orbiting behind the menu; the
last world played (a fixed seed if there are none); the world list can
create, delete (with a confirm step) and rename, and shows details.
