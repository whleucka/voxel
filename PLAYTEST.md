# Play-test checklist

What's waiting on a real play-test, newest first. Tick things off as they
pass; anything that fails, note what happened (a screenshot plus the F12
coordinates helps).

## Zombies and mummies (Alex's idea)

- [ ] Zombies come out at night on grass (not in the desert), and vanish at sunrise
- [ ] Mummies come out at night in the desert only, and are still there in the morning
- [ ] Both are slow: you can outrun them by sprinting, and Thorn Crawlers are clearly faster
- [ ] Zombies sometimes drop a carrot, mummies often drop wool
- [ ] They stay away from torches and lamps, like the other night enemies
- [ ] A friend on the network sees and can fight them
- [ ] Alex's verdict: do they look like a zombie and a mummy?

## Hard difficulty and well fed

- [ ] Settings has Hard; Hard nights are busier and the enemies take more hits and hit harder, but it still feels fair
- [ ] On Normal, after a fight your health doesn't come back until you eat; the hearts look darker until you do
- [ ] Eating shows a loaf next to the hearts; it blinks before running out, and health stops coming back once it's gone
- [ ] You can eat at full health to stock up (up to 10 minutes), then it says "not hungry"
- [ ] On Easy nothing feels different (Alex)
- [ ] Is 45 seconds per food point about right, or do you have to eat too often?
- [ ] A friend on the network sees their own loaf and darker hearts

## Farming (Aria's idea)

- [ ] Make a hoe at a table and till grass and dirt: the soil turns light brown, and grass sometimes pops out a seed
- [ ] Plant wheat, carrot and strawberry seeds (strawberry seeds: 1 Berries at a table)
- [ ] A field next to water is dark brown (wet) without watering; one away from water stays light brown and its crops don't grow
- [ ] Watering can: fill it at water, splash a field (a 3×3 patch goes dark), it runs out after 10 splashes and says so
- [ ] Crops grow through four stages in about 20 minutes on wet soil, and the stages are easy to tell apart
- [ ] Ripe crops give food and seeds; unripe ones give the seed back; bread from 3 wheat
- [ ] Watered soil dries after about 20 minutes: is having to water twice per crop too fiddly?
- [ ] Crops keep growing while you're somewhere else in the world, and after saving and loading
- [ ] A friend on the network sees the field grow
- [ ] Aria's verdict on the art (GIMP sources: `assets/blocks/wheat_*.xcf`, `carrot_*`, `strawberry_*`, `farmland_*`)

## Dark water stays dark

- [x] At midday, a cave pool in the dark is dark too: no flat teal glow
- [x] Next to a glow crystal or torch, the pool shows its blue-green tint
- [x] A lake or the sea under open sky looks just as before, day and night

## Cave pools — new worlds only

- [ ] In a new world's deep caves, water sits in hollows: no lone blocks, none hanging over an edge with its side showing
- [ ] Pools are 2–3 deep; you can swim, and climb or jump out
- [ ] Glowfin show up in cave pools now (old pools were 1 deep, too shallow for them), and you can fish them
- [ ] An old world's caves look just as they did

## Glow crystals that look like crystals

- [ ] In a cave, floor crystals point up and ceiling crystals hang down
- [ ] You can see the cave through the gaps between the shards; a cluster of them looks solid, not hollow
- [ ] Crystals still light up the cave, and Cave Grumbles still keep away
- [ ] Mining one gives you a Glow Crystal; place it on a ceiling and it hangs, on a floor and it stands
- [ ] The hotbar icon looks right
- [ ] Aria's verdict on the art (GIMP sources: `assets/blocks/glow_crystal.xcf`, `glow_crystal_end.xcf`)

## Beds

- [ ] Survival: punch some leaves off a tree, feed them to a Woolly, and get wool (any kind of leaves works)

- [ ] Craft a White Bed (3 wool + 3 planks at a table), then dye one of each colour
- [ ] Place beds facing each way: head goes the way you look; needs floor under both halves
- [ ] Beds look right by day and at night, from every side, in each colour; you can jump on one
- [ ] By day, right-click: "Respawn point set. You can only sleep at night"
- [ ] At night: you lie down, the view drops onto the pillow and the screen darkens; morning comes after a couple of seconds
- [ ] A monster close by: "You can't sleep, monsters are nearby!"
- [ ] W/A/S/D or Space gets you up; getting hit wakes you
- [ ] Die and wake up next to your bed; break it, die again, and you're back at spawn ("Your bed is missing")
- [ ] With a guest: one in bed waits ("1 of 2 in bed"), both in bed skips the night; each sees the other lying in bed; you can't share a bed
- [ ] Save and reload: beds stay, and so does your respawn bed
- [ ] The kids' verdict (Alex: the colours!)

## Doors

- [ ] Craft Wooden Doors (6 planks at a table, makes 2) and place them facing each way
- [ ] Right-click opens and shuts it, top or bottom half; crouch + right-click places a block against it
- [ ] It looks right from both sides, open and shut, by day and at night; you can see out of the window
- [ ] A shut door stops you; an open one lets you through; it won't shut while you stand in it
- [ ] Build a hut with a door and spend a night inside: enemies stay out while it's shut, follow you in when it's open
- [ ] Breaking either half gives one door back; digging the ground under it pops it off
- [ ] Save and reload: doors stay open or shut
- [ ] A guest sees a door open and shut
- [ ] The kids' verdict

## Cave rooms

- [ ] Make a NEW world and explore the caves: now and then a tunnel opens into a big room (10–20 blocks across)
- [ ] Rooms join up with the tunnels, have a floor you can walk on, and never open into the sea or a lake
- [ ] An older world's caves look exactly as before
- [ ] The kids' verdict: big enough? often enough?

## Gem pockets, bedrock floor, ore gates (PRs #39, #40)

- [ ] A wooden pickaxe on diamond ore: it doesn't budge, "Too hard! You need an Iron Pickaxe or better."
- [ ] Make a NEW world: no bedrock in the caves, only at the very bottom
- [ ] Gems are rare, and come in clumps of 3–6 of one kind (mostly in deep cave walls)
- [ ] Aria and Alex's verdict on gem rarity

## Chests and treasure

- [ ] Craft a Chest (8 planks at a table), place it, right-click: a 4 x 5 panel where crafting was
- [ ] Put things in and take them out; shift-click goes both ways
- [ ] Save and reload: the things are still there
- [ ] Breaking a chest drops it and everything in it
- [ ] A guest opens the same chest and sees it change
- [ ] Make a NEW world and explore deep caves (below about y = 82) for a treasure chest
- [ ] Right-click without a key: "Locked! You need a Treasure Key."; it can't be mined
- [ ] Make a Treasure Key (2 gold ingots + a gem at a table) and unlock it: loot!
- [ ] An old world has no treasure chests and looks exactly as before
- [ ] The kids' verdict: too rare? good loot?

## Block outline

- [ ] Aiming at a block within reach shows a thin dark frame round it
- [ ] No frame on water, or when nothing is in reach
- [ ] The frame follows the crosshair smoothly, also on glass, leaves and crystals
- [ ] It goes away while the inventory is open

## Fishing (PR #34)

- [ ] Fish show up: Pebblefish in a lake, Sunfin in the sea, Glowfin in a cave pool (new worlds)
- [ ] Glowfin spots glow in the dark; tails wag while fish swim
- [ ] Fish never jump out of the water; one left on dry land flops and vanishes
- [ ] Hitting a fish does nothing
- [ ] Make a Fishing Rod (3 sticks + 2 wool at a Crafting Table)
- [ ] Right-click casts: the bobber flies out and floats, with a line from your hand
- [ ] On a bite the bobber dips and "Bite! Right-click!" pulses; reeling in shows "Caught a …!"
- [ ] Reeling too early gets nothing; missing a bite just means waiting for the next
- [ ] Casting near swimming fish gets bites quicker
- [ ] Treasure turns up now and then (and the Soggy Boot)
- [ ] Cook a raw fish in the furnace; cooked heals more than raw
- [ ] Switching item or walking away breaks the line
- [ ] The rod's wear bar goes down per catch, and it breaks after 64
- [ ] A guest fishes next to the host, and each sees the other's line
- [ ] Spam-clicking the rod doesn't catch anything faster
- [ ] The kids' verdict on the 1.2 s bite window (easy to tune)

## Tools and weapons (PR #33)

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
