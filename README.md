# Grumblegrove

<img width="1920" height="1200" alt="image" src="https://github.com/user-attachments/assets/4bc2fe34-cb1c-488c-b04a-0df6029a89e9" />

* A hobby voxel survival game, built from scratch in C++ and OpenGL.
* It started as an engine prototype to explore voxel rendering and chunk management, and grew into a game the family plays together on the home network.
* Fun fact: my daughter helped make some of the block textures!

## Features

- **A world to explore:** endless terrain streamed in chunks across plains, mountains, deserts, oceans, tropical islands and cherry groves. It has trees, a day/night cycle, clouds and shadows.
- **Caves:** new worlds have winding tunnels with glow crystals, shallow pools and gems in the walls ([more below](#caves)).
- **Wild plants:** every biome grows its own flowers, ferns, mushrooms, cactus, berry bushes or coral. Flowers make the dyes, and there are torches, stew, cookies and a cake to share ([more below](#wild-plants)).
- **Survival:** break blocks to collect them and place them to build. Watch your health, stamina and breath, and dig for coal, iron, copper, gold, emerald, ruby and diamond.
- **Creatures:**
  - Friendly Woollies, Bunnies, Shell Turtles and Blossom Deer. Feed them and they give you something back: a Woolly eats any leaves and gives you wool.
  - At night, Gloomlings, Thorn Crawlers and Zombies come out, Mummies shuffle through the desert (and stay after sunrise), and Cave Grumbles lurk in the dark underground.
  - Pebblefish, Sunfin and glowing Glowfin swim in lakes, the sea and cave pools.
- **Crafting:** discover recipes as you collect things, and build a Crafting Table for the bigger ones ([more below](#crafting)).
- **Smelting:** a furnace that cooks raw ore into ingots, and sand into glass, while you're off exploring ([more below](#smelting)).
- **Doors:** craft wooden doors, right-click to open and shut them; nothing gets through a closed one ([more below](#doors)).
- **Stairs and slabs:** half-block steps and floors in planks, cobblestone and four kinds of brick; walk up them without jumping ([more below](#stairs-and-slabs)).
- **Fences and ladders:** fence in a garden or a pen with a gate, and climb ladders up walls and down mine shafts ([more below](#fences-gates-and-ladders)).
- **Beds:** craft a bed in one of 8 colours, sleep through the night, and wake up there after you die ([more below](#beds)).
- **Chests:** craft chests to keep your things in, and hunt deep caves for locked treasure chests ([more below](#chests-and-treasure)).
- **Fishing:** make a rod, cast it, and reel in when the bobber dips. Cook your catch, or find treasure ([more below](#fishing)).
- **Pets:** tame a stray Puppy or Kitten with three treats, and it wears a collar, follows you everywhere and sits when you ask ([more below](#pets)).
- **Boats:** make a boat from planks, put it on the water and sail it, with a friend in the front seat ([more below](#boats)).
- **Farming:** till soil with a hoe, plant wheat, carrots and strawberries, keep them watered, and harvest them for food ([more below](#farming)).
- **Tools and weapons:** pickaxes, axes, shovels, swords and hoes from wood to diamond; mining takes time and the best ore needs the best picks ([more below](#tools-and-weapons)).
- **Creative mode:** fly, build without running out, and pick from every block, including planks, bricks, glass, eight bright colors and lamps.
- **Play together:** host a world on your home network, and up to 8 players can join from the title screen.
- **Your own worlds:** create, rename and delete worlds. Each one saves its edits, inventories and mode, by itself every five minutes and when you quit.
- **Engine:**
  - Greedy meshing with frustum culling, multi-threaded chunk generation and deterministic, seeded worldgen.
  - Sky and block lighting, shadow mapping, and transparent and cut-out blocks.
  - A Dear ImGui debug panel (F12).
- **Original art:** block textures hand-made in GIMP, plus generator scripts in `tools/` for the newer tiles.

## Ideas for next time

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
*   **Left Ctrl:** Sprint (uses stamina). **Left Shift:** Crouch, or get out of a boat.
*   **Left click:** Hit a creature, pick up an empty boat, or break a block (hold it in survival).
*   **Right click:** Place a block, put a boat on the water or get into one, feed an animal or pet, eat the food you're holding, cast and reel in a fishing rod, open a chest, furnace or Crafting Table, open and shut a door or a gate, sleep in a bed, pick berries, eat a slice of cake, till, plant and water with a hoe, seeds or a watering can, or use compost.
*   **E:** Inventory. **1-0 / scroll:** Hotbar. **Q / Ctrl+Q:** Drop one / the stack.
*   **F:** Toggle fly mode (creative worlds only). Space and Shift fly up and down.
*   **T:** Chat. Type and press Enter (or click Send); Esc closes it.
*   **P:** Drop a map pin where you stand, or pick up the one next to you.
*   **ESC:** Close the inventory, or open the pause menu (Resume, Settings,
    Save game, Return to title screen, Exit game). Single player pauses.
*   **F5:** Save the world. **F11:** Toggle fullscreen.
*   **F12:** Debug panel (info, stats, where each frame's time goes on the CPU and GPU, wireframe, V-sync and host cheats).

## The map

The minimap in the top-left corner turns with you, so straight ahead is
always up. Outside it shows the land from above; underground it shows the
caves around your feet instead, open space light and rock dark, so you can find
your way back out. A little house marks home (your bed, or where the world
began if you haven't slept in one), and when home is too far to fit, the house
waits on the edge of the map, pointing the way, with how many blocks it is.
Press **P** to drop up to eight coloured pins at places worth finding again,
like a cave mouth. They're saved with you. In multiplayer everyone sees
everyone's pins (other players' pins are diamonds, and only the player who
dropped one can pick it up), and the other players show as arrows in their
shirt colours, at any distance. The map can be turned off in Settings.

## Game modes

Each world is **Survival** or **Creative**, chosen when you create it and
changeable from the world list.

*   **Survival:** break blocks to collect them, place them to use them up,
    and watch your health, stamina and breath. No flying.
*   **Creative:** fly, place without running out, pick any item from the
    palette beside the inventory, and nothing can hurt you.

### Difficulty and food

Pick a difficulty in Settings (the host's applies to everyone):

*   **Peaceful:** no enemies.
*   **Easy:** fewer enemies, weaker and slower.
*   **Normal:** enemies as they're meant to be.
*   **Hard:** half as many enemies again, with half as much health again,
    and they hit harder.

Food heals you right away and also makes you **well fed**: 45 seconds per
point of food, up to 10 minutes (a loaf of Bread is 6 minutes). A loaf
next to your hearts shows you're well fed, and blinks in the last 30
seconds. On **Normal and Hard** your health only comes back by itself
while you're well fed; when it can't, your hearts look darker. On
Peaceful and Easy it always comes back.

### Building blocks

Creative worlds also have blocks made for building: oak, pine, palm and
cherry **planks**, **bricks**, **stone bricks**, see-through **glass**, eight
bright **colored blocks** and a **lamp** that lights up a room. They're in
the palette beside the inventory.

The tiles are original pixel art drawn by `tools/gen_building_tiles.py`.
Each has a GIMP source in `assets/blocks/` (and a layer in
`assets/atlas/block_atlas.xcf`) for anyone who'd like to repaint it.
To try a new version, export your painting over its cell in
`res/block_atlas.png` (32×32 pixels; the cell is the layer's position in
the atlas). Clear pixels show as see-through on glass, leaves and glow
crystals. To go back to the generated art, run
`python3 tools/gen_building_tiles.py --only glow_crystal` (or any tile's name).

## Crafting

Open your inventory (**E**) and the **Crafting** panel is on the left.
Recipes are *discovered*: one shows up once you've held every ingredient,
and the rest stay as **???** until you find what they need. Each player
discovers their own.

*   **Anywhere:** planks from logs, sticks, a **Crafting Table**, a boat,
    **torches** (4 from a coal or charcoal and a stick) and **compost**
    (4 leaves of any kind).
*   **At a Crafting Table** (right-click it, or stand next to it): stone,
    sandstone and snow bricks, white blocks, colored blocks dyed with
    flowers, lamps, the Furnace, stairs and slabs, and the plant foods.

Click a recipe to make one, shift-click to make as many as you can.
Crouch and right-click to build against a table instead of opening it.

## Smelting

Build a **Furnace** (8 cobblestone at a Crafting Table), place it and
right-click it. Put something to smelt in the top-left slot and fuel in
the slot below; it cooks one item every five seconds, keeps going while
you're away, and glows while it burns.

*   **Raw iron, copper and gold** → ingots
*   **Sand** → glass, **cobblestone** → stone, **sandstone** → bricks
*   **Logs** → charcoal

Fuel: coal or charcoal smelts 8 items, a log 2, planks 1½, sticks ½.
Shift-click sends things to the right slot. Breaking a furnace drops
everything in it.

## Tools and weapons

In survival, blocks take time to break: **hold** the left button and a ring
round the crosshair fills up. Bare hands are slow, and stone and ore give
nothing without a pickaxe, so start by punching a tree.

Make tools at a Crafting Table from sticks and a material:

| Tool | Head | Speeds up |
|---|---|---|
| Pickaxe | 3 | stone, ore, bricks, glass, furnaces |
| Axe | 3 | logs, planks, crafting tables, leaves |
| Shovel | 1 | dirt, grass, sand, snow |
| Sword | 2 | fighting |

Materials, worst to best: **wood** (planks), **stone** (cobblestone),
**copper** and **iron** (ingots from a furnace), then **ruby**, **emerald**
and **diamond**. Coal and copper ore need a stone pickaxe, iron and gold a
copper one, and gems an iron one: until you have it, ore won't budge, and
the game tells you which pickaxe you need. Tools wear out (watch the bar under the
icon). Gem tools have a trick each: a ruby sword sets enemies alight, an
emerald pickaxe sometimes doubles ore, and diamond is the fastest and
toughest. Creative breaks everything in one hit, as before.

## Chests and treasure

Make a **Chest** from 8 planks at a Crafting Table, place it and
right-click it: it holds 20 stacks. Shift-click moves things between the
chest and your inventory. Breaking a chest drops everything in it.

Somewhere deep in the caves, far below the sea, there are **treasure
chests**: dark wood with gold bands and a lock. They're rare, there's no
sign pointing to them, and you can't break them open. You need a
**Treasure Key** (2 gold ingots and a ruby, emerald or diamond at a
Crafting Table). Hold it and right-click the chest: the key is used up
and the chest is full of treasure, like ingots, gems, glow crystals, food,
sometimes a good tool, and now and then another key. After that it's an
ordinary chest you can keep using.

Treasure chests are only in worlds made from now on. Older worlds keep
exactly the terrain they had.

## Doors

Make **Wooden Doors** from 6 planks (any wood) at a Crafting Table: you
get two. Place one on the ground with two blocks of room above it, and it
faces you. Right-click either half to open or shut it; crouch to place a
block against it instead. It has a window in the top half, so you can see
who's outside.

A **closed door is a wall** to everything: players, enemies and
animals. Only players can open or break a door, so a house with one is a
safe place to spend the night. Leave it open and enemies can follow you
in. Breaking either half gives the door back.

## Stairs and slabs

Make them at a Crafting Table from oak, pine, palm or cherry **planks**,
**cobblestone**, **stone bricks**, **bricks**, **sandstone bricks** or
**snow bricks**: 6 make 4 **stairs**, and 3 make 6 **slabs**.

*   A **slab** is half a block high. Walk onto it without jumping, or use a
    row of them for a porch or a low floor. Put a slab on top of the same
    kind of slab and the two become one full block.
*   A **stair** is two half steps. It goes down with its high side away
    from you, so walking forward goes up. Walk up a staircase without
    pressing jump. From behind or the sides it's a wall a block tall.

Creatures walk up them too. They break like the block they're made of,
and a stair always gives back the same stair, whichever way it faced.

## Fences, gates and ladders

All three are made at a Crafting Table from planks and **Sticks**.

| Recipe | Makes |
|---|---|
| 4 planks of one wood + 2 Sticks | 3 Oak, Pine, Palm or Cherry **Fences** |
| 2 planks of any wood + 4 Sticks | 1 **Fence Gate** |
| 7 Sticks | 3 **Ladders** |

*   **Fences** join up by themselves, to other fences, to a gate in line
    with them, and to walls. They're too tall to jump over, for you and for
    creatures, so a fenced pen keeps Woollies and stray pets in and enemies
    out.
*   A **Fence Gate** goes across the way you're looking. Right-click it to
    open and shut it. Only players can open gates, and a gate won't shut
    on someone standing in it.
*   A **Ladder** goes on the side of a block. Walk into it (or hold jump)
    to climb, crouch to hold on, and let go to slide slowly down. You don't
    get hurt falling onto a ladder. If the block behind it is broken, the
    ladder drops off.

## Beds

Make a **White Bed** from 3 wool (feed a Woolly some leaves) and 3 planks
(any wood) at a Crafting Table, then dye it with one of the coloured blocks' dyes for a **red**
(poppy), **orange** (hibiscus), **yellow** (buttercup), **green** (cactus),
**blue** (cornflower), **purple** (lavender) or **pink** (pink tulip) one.
Place it on the floor: its head goes the way you're looking. A bed is low,
so you can climb on and bounce about on it.

Right-click a bed and it becomes **your respawn point**: die, and you wake
up next to it. At **night**, right-clicking also puts you to bed. Once
**every player** is in bed, the night skips to 6:30 in the morning. You
can't sleep with a monster within 8 blocks, or in someone else's bed. Press
a movement key or jump to get up, and getting hurt wakes you. Break your bed
and you go back to the world spawn; breaking either half gives the bed back.

## Fishing

Fish swim in the world's water: **Pebblefish** in lakes, **Sunfin** in the
sea and round tropical islands, and **Glowfin**, with glowing spots, in dark
cave pools. They can't be hurt, and they never leave the water.

Make a **Fishing Rod** at a Crafting Table from 3 sticks and 2 wool (a
Woolly gives you wool). Right-click to cast, and watch the bobber. After a
while it dips under and **"Bite! Right-click!"** appears: right-click quickly
to reel in. Too early and the hook comes back empty; miss it and just wait
for the next bite. Cast where you can see fish swimming and they bite twice
as fast.

You'll mostly catch the fish that lives in that water, but now and then
something else: a shell, some sticks, a soggy boot, or, if you're lucky,
gold, an emerald or a diamond. Raw fish heal a little. Cook them in a
furnace and they heal a lot more. A rod lasts 64 catches.

## Boats

Make a **Boat** from 5 planks of any wood. You don't need a Crafting Table,
so you can make one at the shore. Right-click the water with it, and it
floats there facing the way you're looking.

Right-click the boat to get in. The first one in **drives** from the back
seat and a friend can ride in the front. **W** goes, **S** slows down and
then backs up, and the boat turns toward where you're looking. It's faster
than sprinting, and it doesn't tire you out. **Shift** gets you out, onto
the shore if there's some close by, or into the water beside the boat.

From a boat you can look all round, fish, eat and use your inventory, but
not build, break or fight. A boat stops at the shore and won't move on land.
Hit an empty boat to pick it up again. Boats never break, and a boat you
leave stays where you left it, even after you quit. Quit while you're in one
and you'll be back in your seat when you return.

## Pets

Stray **Puppies** wander the plains and mountains, and stray **Kittens**
the plains, cherry groves and desert. Give one three treats, right-clicking
it with each, and it's yours. It gets a coloured collar, and a heart pops up
with every treat.

| Pet | Treats (any one) |
|---|---|
| Puppy | Berries, Strawberry or Bread |
| Kitten | Any raw fish |

Your pet follows you around. If you run, fly or sail far ahead, or it gets
stuck, it catches up and turns up beside you. Right-click it with anything
that isn't a treat to make it **sit** and stay. Do it again and it follows
you. Pets can't be hurt, never wander off, and are still there when you come
back to the world. Each player can have up to 6 pets, and a friend's pet
only sits for them.

## Farming

A play-tester's idea. Make a **hoe** at a Crafting Table from 2 of a tier's material
(planks, cobblestone, an ingot or a gem) and 2 sticks. Right-click **grass
or dirt** with it to till it into **farmland**. Tilling grass sometimes turns
up **Wheat Seeds** or **Carrot Seeds**; for **Strawberry Seeds**, take 1
Berries (from an animal that eats Cherry Blossom) to a table for 2 seeds.
Right-click farmland with seeds to plant them.

Crops only grow on **wet** soil, which is dark brown; dry soil is light
brown, and crops on it just wait. Farmland within **4 blocks of water** stays
wet by itself. Anywhere else, use a **Watering Can** (3 copper ingots at a
table): right-click water to fill it, then right-click farmland or a crop to
wet the 3×3 patch round it. A full can holds 10 splashes, and watered soil
stays wet for a day (20 minutes).

On wet soil a crop goes from sprout to ripe in about a day, and keeps growing
while you're away, as long as the world is open. Break a **ripe** crop to
harvest it: **wheat** gives 1 Wheat and 1–2 seeds (3 Wheat make **Bread** at a
table), **carrots** and **strawberries** give 2–3 to eat and a seed. Break one
before it's ripe and you just get the seed back. A block put on farmland
packs it back into dirt.

The crop and soil tiles are placeholders with GIMP sources in `assets/blocks/`,
for repainting.

## Wild plants

Worlds made from now on grow **wild plants**, different in every biome, so
you can tell where you are from what's growing:

| Biome | What grows |
|---|---|
| Plains | tall grass, patches of poppies, buttercups and cornflowers |
| Cherry groves | tall grass, pink tulips and daisies |
| Mountains | ferns, lavender, and red and brown mushrooms under the pines |
| Tropics | hibiscus and **berry bushes** |
| Desert | **cactus** (it pricks!) and dead bushes |
| Ocean | pink, blue and yellow **coral** on the sea floor |
| Beaches | **shells** at the waterline |
| Caves | brown mushrooms |

Plants break at once and you walk through them. Break the ground under
one and it pops off. Put them back on the ground they grow on (flowers on
grass, cactus on sand). Tall grass sometimes gives a Wheat Seed, a dead bush
a stick, and shells a Sea Shell.

*   **Dyes:** each colour comes from its flower: red poppy, orange
    hibiscus, yellow buttercup, green cactus, blue cornflower, purple
    lavender, pink tulip. 8 white blocks and a flower make 8 coloured
    ones. **White blocks** come from 8 sand and wool, a daisy or a sea shell.
*   **Berry bushes:** right-click a ripe one for 2 Berries. It ripens again
    in 5 minutes.
*   **Torches:** right-click the floor or the side of a block. They light up
    caves and keep monsters away.
*   **Food:** **Mushroom Stew** (a red and a brown mushroom), **Berry
    Cookies** (4 from 2 wheat and a berry), and a **Cake** (3 wheat, 2
    strawberries and sweet petals). Put the cake down and anyone hungry can
    right-click it for a slice; it lasts 4 slices.
*   **Compost:** right-click a growing crop to grow it a stage, a picked
    bush to ripen it, or grass or sand to sprout that biome's plants. It
    works in older worlds too, which is how they get flowers.
*   **Bricks:** 4 sandstone or 4 snow make 4 bricks (snow bricks make good
    igloos).

The plant tiles are placeholders with GIMP sources in `assets/blocks/`,
for repainting.

## Caves

Worlds made from now on have **natural caves**: winding tunnels, some
opening onto hillsides so you can walk straight in, and now and then
opening out into a **room** 10–20 blocks across, big enough for an
underground base. Deep inside you'll find
**glow crystals** (break one to take its light home), clusters of shards
growing up from the floor and hanging from the ceiling, **pools** filling
hollows in the floor (2–3 deep, so mind your breath), and
ore in the walls. Deep down, far below the sea, there are rare **gem
pockets**: a clump of rubies, emeralds or diamonds together. Most are in a
cave wall, the rest buried in the rock. Bedrock is only the floor of the
world. Cave Grumbles only appear
in the dark, so a crystal, lamp or torch keeps the area around it safe.

Worlds made before caves keep exactly the terrain they had: each world
remembers which generator made it.

## Playing together (home network)

The game opens on a title menu, with your last world slowly circling behind
it. Type your name, then:

*   **Single player:** pick a world from your list (or create, rename,
    change the mode of, or delete one), then **Play**, or **Host on network** to let others on your
    network join. F12 shows the address and port guests can use.
*   **Multiplayer:** games on your network are listed automatically; if yours
    isn't, type the host's address (e.g. `192.168.1.20`).
*   **Settings:** difficulty, render distance, field of view, mouse
    sensitivity, fullscreen, vsync, clouds and shadows. Also in the pause menu.

Up to 8 players. Press **T** to chat: messages show bottom-left with the
time and who said them, along with news like who joined, who's sleeping
and who was beaten by a Zombie. Guests keep their inventory in the host's
world under their name. The host needs TCP port 47800 and UDP port 47801 open. Only the host
can use the F12 cheats. Guests play in the host's world mode.


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
