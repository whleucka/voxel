# Play-test checklist

What's waiting on a real play-test, newest first. Tick things off as they
pass; anything that fails, note what happened (a screenshot plus the F12
coordinates helps).

## Tools and weapons (equipment)

- [ ] Survival: punching a tree is slow (about 3 s a log); the ring fills round the crosshair
- [ ] Stone by hand takes ages and drops nothing; a wooden pickaxe gets cobblestone
- [ ] Stone pickaxe → coal and copper; copper pickaxe → iron and gold; iron pickaxe → gems
- [ ] Axe on logs and shovel on dirt are much faster than a pickaxe on them
- [ ] Wear bar shows on a used tool; a tool breaking shows "Your … broke!"
- [ ] Bare hands vs. a Grumble (16 punches!) vs. a sword
- [ ] Ruby sword: the enemy glows orange and keeps taking damage
- [ ] Emerald pickaxe: sometimes two of an ore
- [ ] Holding the button after hitting a creature doesn't dig the ground behind it
- [ ] Creative still breaks everything in one hit
- [ ] The creative palette fits on screen (it got smaller cells for all the tools)
- [ ] A guest mines and fights the same way
- [ ] The kids' verdict: too hard? too easy? (the numbers are easy to tune)

## Smelting (PR #32)

- [ ] Make a Furnace (8 cobblestone at a Crafting Table) and place it
- [ ] Right-click it: the Furnace panel shows where the crafting panel was
- [ ] Smelt raw iron with coal: the arrow fills and the flame burns down
- [ ] Walk away, come back: the ingots are done
- [ ] A lit furnace glows at night
- [ ] Sand → glass, cobblestone → stone, sandstone → bricks, log → charcoal
- [ ] Shift-click sends ore to the input and coal to the fuel slot
- [ ] Breaking a full furnace drops everything in it
- [ ] Save and reload while it's burning: it carries on
- [ ] A guest uses the host's furnace (and both see it update)

## Hotkeys (PR #31)

- [ ] F12 opens and closes the debug panel; F1, F2, F3 do nothing
- [ ] F toggles flying in creative; T does nothing

## Crafting (PR #30)

- [ ] Survival from scratch: punch a log → planks → Crafting Table → place it → right-click it → stone bricks
- [ ] Crafting panel looks right (left of the inventory) and scrolls
- [ ] `???` rows unlock with a "New recipe" popup
- [ ] Table recipes are dimmed away from a table and work next to one
- [ ] Dyes make colored blocks (ruby → red, sea shell → blue, …)
- [ ] A guest crafts at the host's table
- [ ] The kids' verdict: too easy?

## Falling out of the world (PR #29)

- [x] The creative test world loads with the player on the ground

## Light stalls (PR #28)

- [x] The title screen loads quickly and looks right
- [ ] A new creative world plays normally, including flying fast
- [ ] Crystal light crosses chunk borders smoothly in caves
- [ ] Placing and breaking blocks near crystals feels instant

## Caves (PR #26) — new worlds only

- [ ] A new world has caves you can walk into from a hillside (head inland)
- [ ] Glow crystals below about y = 87, pools below y = 77, gems in the walls
- [ ] Puddles are shallow: the breath bar never drops
- [ ] Breaking a crystal in survival gives you the crystal
- [ ] Grumbles show up in dark tunnels, but not near crystals or lamps
- [ ] An old world looks exactly as before
- [ ] A guest sees the same caves as the host
- [ ] Chunk loading still feels smooth

## Building blocks (PR #25)

- [ ] Every new block is in the creative palette and looks right in the world and hotbar
- [ ] A glass wall shows what's behind it, with no lines between panes
- [ ] A lamp lights up a dark room
- [ ] On the LAN, a guest sees blocks the host places
- [ ] Aria's verdict on the colors
