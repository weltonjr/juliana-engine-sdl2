# Aeterium — Gameplay Specification

## Vision

Aeterium is a spiritual successor to Clonk Endeavor: a 2D sandbox platformer where the
world is a destructible bitmap. The player is an explorer/engineer who digs into the earth,
blasts ore veins, collects the fragments, refines them, and uses the materials to build
structures and expand their operation — all while the environment fights back.

The core fantasy: **you are a one-person mining company inside a living planet.**

---

## Core Loop

```
EXPLORE ──▶ DIG ──▶ BLAST ORE ──▶ COLLECT FRAGMENTS
   ▲                                        │
   │                                        ▼
EXPAND ◀── BUILD ◀── REFINE ◀──────── CARRY TO BASE
```

1. **Explore** — scout the terrain for ore deposits and good dig sites
2. **Dig** — carve tunnels through dirt, expose rock, reach ore veins
3. **Blast** — use explosives to shatter ore into physical fragments
4. **Collect** — pick up fragments (limited carry weight)
5. **Carry** — transport fragments back to base (tunnels you dug become roads)
6. **Refine** — deposit into a refinery building, get ingots/components
7. **Build** — construct new structures (smelters, storage, ladders, platforms)
8. **Expand** — use new structures to dig deeper, access new materials

---

## Player Character

### Movement

| Action | Input | Notes |
|---|---|---|
| Walk left / right | A / D | Accelerates, has friction |
| Jump | Space | Has jump buffer (press slightly before landing = still jumps) |
| Coyote jump | Space | Can jump ~100ms after walking off a ledge |
| Climb slope | (automatic) | Steps up to 2 cells (8px) without input |
| Dig down | Hold C + facing ground | Removes cells in a radius below |
| Dig horizontal | Hold C + moving | Removes cells in front of character |
| Carry | (automatic when near fragment) | Reduces move speed, limits jump height |
| Interact | E | Use/deposit at buildings |

### Character States (FSM)

```
IDLE ──▶ WALK ──▶ RUN
  │         │
  └──▶ JUMP ◀── FALL
          │
         DIG
          │
        CARRY
          │
        INTERACT
```

- **IDLE**: Standing still on ground
- **WALK**: Moving at normal speed
- **RUN**: Moving faster (hold sprint key — TBD)
- **JUMP**: Rising after jump input; transitions to FALL when velocity.y > 0
- **FALL**: In the air with downward velocity; coyote timer active briefly after leaving ground
- **DIG**: Actively removing terrain cells; character slows or stops
- **CARRY**: Holding an ore fragment; weight affects physics
- **INTERACT**: Using a building (short animation lock)

### Physics Constants (v0.1)

| Parameter | Value | Meaning |
|---|---|---|
| Walk speed | 140 px/s | Horizontal velocity while walking |
| Acceleration | 800 px/s² | How fast the character reaches walk speed |
| Friction | 600 px/s² | Deceleration when no input |
| Jump impulse | −380 px/s | Upward velocity on jump |
| Gravity | 900 px/s² | Downward acceleration always active |
| Max fall speed | 1400 px/s | Terminal velocity |
| Coyote time | 6 frames | Can still jump after leaving ground |
| Jump buffer | 6 frames | Jump is queued if pressed slightly before landing |
| Dig radius | 3 cells (12px) | Radius of terrain removed per dig action |
| Step-up max | 8px (2 cells) | Maximum slope the character auto-climbs |

---

## Materials

| ID | Name | Dig-able | Explode-able | Notes |
|---|---|---|---|---|
| 0 | EMPTY | — | — | Void / already dug |
| 1 | DIRT | Yes (fast) | Yes | Most common; terrain surface |
| 2 | ROCK | No (v0.1) | Yes | Requires explosives; structural |
| 3 | GOLD_ORE | No direct | Yes | Shatters into fragments with gold value |
| 4 | WATER | — | — | Post-MVP; fluid simulation |
| 5 | AIR | — | — | Reserved; sky zone |

Future materials planned: COAL, IRON_ORE, CRYSTAL, LAVA, OBSIDIAN, WOOD.

---

## Ore Fragmentation (v0.2)

This is the gameplay differentiator. When an explosion hits an ore vein:

1. **Flood fill** — find all contiguous GOLD_ORE cells connected to the blast point
2. **Voronoi partition** — split the region into N irregular sub-fragments (N = ceil(area/threshold))
3. **Spawn fragments** — each sub-fragment becomes a physical rigid body:
   - Its visual is the actual pixels from the bitmap (you see the veins)
   - Its collision shape is a convex polygon (approximate outline)
   - Its mass is proportional to its pixel area
   - It gets an initial outward velocity + random spread
4. **Clear terrain** — all cells that became fragments are set to EMPTY, chunks marked dirty

The player can then:
- **Collect** fragments by walking over them or interacting
- **Carry** them (limited by total weight, not count)
- **Stack** them in a storage building
- **Deposit** into a refinery to get processed ingots

---

## Buildings (v0.2+)

Buildings are kinematic rigid bodies (mass = infinite). They:
- Are placed by the player on flat terrain
- Have an HP bar; take damage from explosions
- Have input/output slots for items
- Can emit fields (a refinery emits heat, a mana crystal emits mana field)

| Building | Function |
|---|---|
| Storage Chest | Holds fragments/ingots; increases carry limit nearby |
| Refinery | Converts ore fragments → ingots (time-based) |
| Ladder | Lets player climb vertical walls |
| Platform | Thin solid floor, falls through |
| Explosive Barrel | Placed, triggered by fire/impact; larger blast radius |
| Campfire | Emits heat field; cooks food (future) |

---

## Combat / Enemies (v0.3+)

- **Cave creatures**: spawn in deep rock; disturbed by explosions and digging
- **Surface wildlife**: passive until player approaches; drops food/materials
- **Golems**: rock-based constructs; immune to digging, vulnerable to explosives
- Player weapon: **pickaxe** (melee dig + light damage), **explosive charges** (crafted)

---

## Win / Progression (TBD)

Aeterium is sandbox-first. Progression options under consideration:
- **Depth milestone**: reach a specific underground layer → boss encounter
- **Crafting tree**: unlock new buildings by discovering rare materials
- **Colony mode**: start with 1 character, build infrastructure to support more
- **Survival mode**: surface degrades over time (cracks, floods), must build supports

---

## UI / HUD (v0.1)

- FPS counter (top-left)
- Character FSM state name (top-left, below FPS)
- Control hints (bottom, subtle)
- F1: Debug overlay (chunk grid, dirty flags, physics bounds)
- F2: Free camera (arrow keys pan without character)
