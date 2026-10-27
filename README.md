# Grumblegrove

<img width="1920" height="1200" alt="image" src="https://github.com/user-attachments/assets/4bc2fe34-cb1c-488c-b04a-0df6029a89e9" />

* A hobby voxel survival game, built from scratch in C++ and OpenGL.
* It started as an engine prototype to explore voxel rendering and chunk management, and grew into a game the family plays together on the home network.
* Fun fact: my daughter helped make some of the block textures!

## Features

- **A world to explore:** endless terrain streamed in chunks across plains, mountains, deserts, oceans, tropical islands and cherry groves. It has trees, a day/night cycle, clouds and shadows.
- **Caves:** new worlds have winding tunnels with glow crystals, shallow pools and gems in the walls ([more below](#caves)).
- **Survival:** break blocks to collect them and place them to build. Watch your health, stamina and breath, and dig for coal, iron, copper, gold, emerald, ruby and diamond.
- **Creatures:**
  - Friendly Woollies, Bunnies, Shell Turtles and Blossom Deer. Feed them and they give you something back.
  - At night, Gloomlings and Thorn Crawlers come out, and Cave Grumbles lurk in the dark underground.
- **Crafting:** discover recipes as you collect things, and build a Crafting Table for the bigger ones ([more below](#crafting)).
- **Creative mode:** fly, build without running out, and pick from every block, including planks, bricks, glass, eight bright colors and lamps.
- **Play together:** host a world on your home network, and up to 8 players can join from the title screen.
- **Your own worlds:** create, rename and delete worlds. Each one saves its edits, inventories and mode.
- **Engine:**
  - Greedy meshing with frustum culling, multi-threaded chunk generation and deterministic, seeded worldgen.
  - Sky and block lighting, shadow mapping, and transparent and cut-out blocks.
  - A Dear ImGui debug panel (F1).
- **Original art:** block textures hand-made in GIMP, plus generator scripts in `tools/` for the newer tiles.

## Ideas for next time

- **Pets:** tame an animal by feeding it, and it follows you around.
- **Tools and smelting:** pickaxes and swords from wood to gems, and a furnace for ore *(in progress)*
- **Doors, slabs and stairs**
- **Treasure chests** hidden around the world
- **Your own look:** pick a color and a hat, so players can tell each other apart on the network
- **Quick-chat:** a wheel of "Hi!", "Follow me!" and "Help!" messages, with no typing needed
- **Hide-and-seek:** a network mini-game

---

## Getting Started

These instructions will get you a copy of the project up and running on your local machine for development and testing purposes.

### Prerequisites

You will need a C++ compiler (like g++), `make`, and the necessary OpenGL and GLFW development libraries installed on your system.

For Debian/Ubuntu-based systems, you might need (I use arch btw):

```bash
sudo apt update
sudo apt install build-essential libglfw3-dev libgl-dev libx11-dev libxrandr-dev libxi-dev libglm-dev
```

Arch

```bash
sudo pacman -S glm glfw
```

### Building

1.  **Clone the repository:**
    ```bash
    git clone https://github.com/whleucka/voxel.git
    cd voxel
    ```

2.  **Compile the project:**
    ```bash
    make
    ```
    This will compile all source files and create the executable in the `bin/` directory.

### Testing

```bash
make test
```

This builds and runs the unit tests in `tests/` (doctest). They exercise the
game simulation directly and never open a window, so they also run in CI.

### Running

After successful compilation, you can run the game using:

```bash
./bin/app
```

Drawing blocks couldn't be *that* hard... *Right?*

## Controls

*   **W, A, S, D:** Move. **Space:** Jump. **Mouse:** Look around.
*   **Left Ctrl:** Sprint (uses stamina). **Left Shift:** Crouch.
*   **Left click:** Break a block, or hit a creature.
*   **Right click:** Place a block, feed an animal, or eat the food you're holding.
*   **E:** Inventory. **1-0 / scroll:** Hotbar. **Q / Ctrl+Q:** Drop one / the stack.
*   **T:** Toggle fly mode (creative worlds only). Space and Shift fly up and down.
*   **ESC:** Close the inventory, or open the pause menu (Resume, Settings,
    Save game, Return to title screen, Exit game). Single player pauses.
*   **F5:** Save the world. **F11:** Toggle fullscreen.
*   **F1:** Debug info and stats. **F2:** Wireframe. **F3:** V-sync.

## Game modes

Each world is **Survival** or **Creative**, chosen when you create it and
changeable from the world list.

*   **Survival:** break blocks to collect them, place them to use them up,
    and watch your health, stamina and breath. No flying.
*   **Creative:** fly, place without running out, pick any item from the
    palette beside the inventory, and nothing can hurt you.

### Building blocks

Creative worlds also have blocks made for building: oak, pine, palm and
cherry **planks**, **bricks**, **stone bricks**, see-through **glass**, eight
bright **colored blocks** and a **lamp** that lights up a room. They're in
the palette beside the inventory.

The tiles are original pixel art drawn by `tools/gen_building_tiles.py`.
Each has a GIMP source in `assets/blocks/` (and a layer in
`assets/atlas/block_atlas.xcf`) for anyone who'd like to repaint it.

## Crafting

Open your inventory (**E**) and the **Crafting** panel is on the left.
Recipes are *discovered*: one shows up once you've held every ingredient,
and the rest stay as **???** until you find what they need. Each player
discovers their own.

*   **Anywhere:** planks from logs, sticks, and a **Crafting Table**.
*   **At a Crafting Table** (right-click it, or stand next to it): stone
    bricks, white blocks, colored blocks dyed with gems and things animals
    give you, and lamps.

Click a recipe to make one, shift-click to make as many as you can.
Crouch and right-click to build against a table instead of opening it.

## Caves

Worlds made from now on have **natural caves**: winding tunnels, some
opening onto hillsides so you can walk straight in. Deep inside you'll find
**glow crystals** (break one to take its light home), shallow **pools**, and
gems in the walls, better ones the deeper you go. Cave Grumbles only appear
in the dark, so a crystal or lamp keeps the area around it safe.

Worlds made before caves keep exactly the terrain they had: each world
remembers which generator made it.

## Playing together (home network)

The game opens on a title menu, with your last world slowly circling behind
it. Type your name, then:

*   **Single player:** pick a world from your list (or create, rename,
    change the mode of, or delete one), then **Play**, or **Host on network** to let others on your
    network join. F1 shows the address and port guests can use.
*   **Multiplayer:** games on your network are listed automatically; if yours
    isn't, type the host's address (e.g. `192.168.1.20`).
*   **Settings:** difficulty, render distance, field of view, mouse
    sensitivity, fullscreen, vsync, clouds and shadows. Also in the pause menu.

Up to 8 players. Guests keep their inventory in the host's world under their
name. The host needs TCP port 47800 and UDP port 47801 open. Only the host
can use the F1 cheats. Guests play in the host's world mode.


## Contributing

Contributions are welcome! If you have suggestions for improvements, bug fixes, or new features, please feel free to open an issue or submit a pull request.

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Acknowledgments

*   [LearnOpenGL.com](https://learnopengl.com/): Learning OpenGL resource.
*   [gimp](https://www.gimp.org): GNU Image Manipulation Program.
*   [GLFW](https://www.glfw.org/): For window and input management.
*   [GLAD](https://glad.dav1d.de/): For loading OpenGL function pointers.
*   [Dear ImGui](https://github.com/ocornut/imgui): For the immediate-mode GUI used for debugging.
*   [stb_image](https://github.com/nothings/stb/blob/master/stb_image.h): For easy image loading.
