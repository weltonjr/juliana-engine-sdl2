# Aeterium — Algorithms Specification

This document explains the algorithms used (and planned) in Aeterium.
It is written so that someone without a math or physics background can understand
the *why* and *how* of each algorithm, not just the formula.

---

## 1. Terrain Generation (v0.1)

### Goal
Generate a believable underground world from scratch — sky on top, dirt in the middle,
rock near the bottom, and irregular gold ore veins scattered through the rock layer.

### Layered Threshold

The simplest part: we divide the map into horizontal zones based on Y position.

```
Y = 0                       ← top of map
Y < 20%  →  AIR             (sky / open air)
Y < 35%  →  DIRT (surface)  (topsoil)
Y < 70%  →  DIRT            (main terrain)
Y < 80%  →  mix DIRT/ROCK   (transition zone)
Y ≥ 80%  →  ROCK            (bedrock)
```

**Why thresholds?** Because real terrain has layers that run mostly horizontally
(like geological strata). This is the simplest model that captures that feel.

### Noise — Making Edges Irregular

Pure thresholds give perfectly flat layers, which looks fake. We add **noise**:
a function that returns a different value at every (x, y) point but does so in a
smooth, organic way (nearby points have similar values — unlike random, where neighbors
are unrelated).

**Simplex noise** (or Perlin noise) is the standard tool. Think of it as a mathematical
"bumpy surface" — sample it at (x, y) and you get a number between −1 and +1.
We use it to perturb the layer boundary:

```
surface_y_at(x) = BASE_SURFACE_Y + noise(x * 0.01, 0) * AMPLITUDE
```

The `0.01` is the **frequency** — lower = smoother, wider hills.
The `AMPLITUDE` controls how tall the hills are in pixels.

We apply the same idea to the dirt/rock boundary, making it wavy.

### Ore Veins

Gold ore veins look like irregular blobs scattered through the rock layer. We generate
them procedurally:

1. Pick N random center points within the rock zone
2. For each center, mark cells as GOLD_ORE if they are within an irregular "blob radius"

The blob radius is computed per-cell using noise again:
```
blob_radius(cx, cy) = BASE_RADIUS + noise(cx * 0.05, cy * 0.05) * VARIATION
mark as ORE if: distance(cell, center) < blob_radius(cx, cy)
```

This gives blobs that have rough edges — more natural than perfect circles.

**Why not just random cells?** Because real ore deposits are connected regions.
Players expect a vein to be a blob they can dig around, not isolated specks.

---

## 2. Per-Cell Color Variation

### Goal
Each material (dirt, rock, gold ore) looks slightly different cell by cell —
avoiding the flat, uniform look of a solid color.

### Hash-based variation (no noise texture needed)

We use a **deterministic hash** of the cell's (x, y) coordinates to produce a
brightness offset. A hash function takes an input and produces a seemingly random
output — but the same input always gives the same output. This is what "deterministic"
means: rebuild the map from scratch and every cell is the same color.

```cpp
unsigned int h = cx * 2654435761u ^ cy * 2246822519u;
int variation = (h & 0x1F) - 16;   // value from −16 to +15
```

The two big prime numbers (`2654435761` and `2246822519`) are "magic multipliers"
from the Knuth hash — they scatter bits well, preventing patterns.

We then add `variation` to each RGB channel of the material's base color. The result:
adjacent cells of the same material have slightly different shades, giving the terrain
texture without needing a texture file.

---

## 3. Chunk Dirty Flag System

### Goal
Rebuilding the GPU texture for every terrain cell every frame would be extremely slow.
We only rebuild chunks that actually changed.

### How it works

The map is divided into a grid of 64×64 cell "chunks". Each chunk has a boolean:
`dirty_visual = true` means "my GPU texture is out of date."

When any cell changes (via digging):
1. Compute which chunk it belongs to: `chunk_x = cell_x / 64`
2. Set `chunk.dirty_visual = true`

Once per frame, before rendering:
- Iterate all 32 chunks
- If `dirty_visual`, rebuild the GPU texture for that chunk
- Set `dirty_visual = false`

**Why chunks and not individual cells?** Uploading data to the GPU has overhead per
call (CPU→GPU transfer). Batching 64×64 cells into one upload is much more efficient
than 4096 individual uploads.

**Why not one big texture for the whole map?** Partially updating a large texture is
possible but complex. The chunk system means we only upload the changed portion.

---

## 4. AABB Terrain Collision

### What is AABB?

AABB stands for **Axis-Aligned Bounding Box** — a rectangle whose sides are parallel to
the X and Y axes (not rotated). For the character, this is the invisible hit box.

### The Core Idea: Move → Overlap → Push Out

The physics update follows three steps per frame:

```
1. Apply gravity and input to velocity
2. Move position by (velocity × dt)
3. Check: does the new position overlap any solid terrain cell?
4. If yes: push position back out until there's no overlap
```

### Step by step for X movement

After moving the character right by some amount:

```
Old position:    [___]
Terrain cell:           [■■■]
New position:    [___]→→[■■■]  ← overlapping!
```

We measure the **penetration depth** — how far inside the wall we are:
```
pen = character_right_edge - wall_left_edge
```

Then push the character back:
```
position.x -= pen
velocity.x  = 0
```

We do the same for moving left, and again separately for Y (up/down).

**Why X before Y?** To prevent diagonal sticking. If we resolve both axes simultaneously,
a character hitting a wall near the floor might be pushed upward instead of sideways.
Resolving X first, then Y, handles each axis independently and gives correct behavior.

### Cell sampling

Instead of checking every cell in the map, we only check the cells that the bounding box
could possibly overlap:

```
cell column on the right: cx = (character_right) / CELL_SIZE
rows to check: from (character_top / CELL_SIZE) to (character_bottom / CELL_SIZE)
```

For a character of size 12×20px and CELL_SIZE=4:
- At most 5 cells checked per axis → 10 cell lookups total per physics tick

---

## 5. Slope / Step-Up Traversal

### Problem
Pure AABB collision stops the character completely when it hits any solid cell,
even a 1-cell bump (4px). This makes movement feel stiff and unnatural.

### Solution: Try lifting before blocking

When the character tries to move right and hits a wall:

```
Before lifting:    [P]→[■]   ← blocked
After lifting 4px: [P]      ← clear path!
                    → [■]
```

The algorithm:
1. Detect that X motion is blocked
2. Check: is there solid ground directly below the character? (are we standing?)
3. If yes: try lifting character 1px at a time, up to `STEP_MAX = 8px`
4. At each lifted position, check again: is X still blocked?
5. If not blocked: keep the new Y and allow the X movement (slope climbed!)
6. If still blocked after all steps: restore Y, stop X movement normally

This handles walking up slopes (the terrain cell boundary creates a 1px step every
CELL_SIZE pixels in slope angle) and mounting ledges up to 2 cells tall.

**Why 8px max?** That's 2 terrain cells. A step taller than that should require jumping.
Too large a step-up value would let the character teleport through thin floors.

---

## 6. Exponential Lerp (Camera Smoothing)

### What is lerp?

**Lerp** = Linear interpolation. To move smoothly from A to B:
```
position += (target - position) * speed_factor
```

Each frame, the camera moves a fixed *fraction* of the remaining distance.
This is called **exponential lerp** because mathematically it decays exponentially
— the camera is fast when far away and slows as it approaches.

### Formula used

```cpp
float t = 1.0f - pow(0.01f, dt * speed);
camera_x += (target_x - camera_x) * t;
```

Breaking it down:
- `pow(0.01, dt * speed)`: how much of the gap to *leave* after `dt` seconds
- `1 - that`: how much of the gap to *close* this frame
- With `speed = 8.0` at 60fps: closes roughly 80% of remaining distance per second

**Why this formula and not a fixed pixel/second speed?**
Fixed speed looks robotic: the camera always moves at the same pace even if
the character just teleported. Exponential lerp naturally accelerates when the target
is far and decelerates as it closes in — like a spring, but without oscillation.

### Bounds clamping

After lerp, we clamp the camera so it never shows outside the map:
```
camera_x = clamp(camera_x, 0, map_width - screen_width)
camera_y = clamp(camera_y, 0, map_height - screen_height)
```

---

## 7. Fixed Timestep Game Loop

### The problem with variable timestep

Games render at different speeds on different machines. If physics runs with
"however much time has passed since last frame" (variable dt):
- At 30fps: dt = 33ms, character jumps high
- At 120fps: dt = 8ms, character jumps lower (physics runs 4× more often)
- On a stutter: dt = 100ms, character teleports through walls

### The solution: accumulator pattern

```
accumulator += frame_time_elapsed

while (accumulator >= PHYSICS_DT) {
    update_physics(PHYSICS_DT)   // always exactly 1/60 second
    accumulator -= PHYSICS_DT
}

render()   // runs at display rate, not physics rate
```

Physics always uses exactly `PHYSICS_DT = 1/60 second`. If the computer is slow and
two frames worth of time passed, physics runs twice. If the monitor is 120Hz, physics
still only runs once (accumulator doesn't fill fast enough for two steps).

**Result**: identical physics behavior at any frame rate. Jump arcs, gravity, dig timers
— all deterministic regardless of performance.

---

## 8. Ore Fragmentation — Flood Fill (v0.2)

### Goal
When an explosion hits a gold ore vein, find all cells that are connected to the
blast point and belong to the same ore deposit.

### Flood fill (BFS)

This is like the "paint bucket" tool in an image editor: starting from one pixel,
spread to all adjacent pixels of the same color.

```
1. Start at blast center cell
2. If cell is GOLD_ORE, add to queue
3. For each cell in queue:
   a. Mark as "visited"
   b. Check all 4 neighbors (up, down, left, right)
   c. If neighbor is GOLD_ORE and not visited → add to queue
4. Result: all connected GOLD_ORE cells
```

We use **BFS (Breadth-First Search)** so we process cells in order of distance from
the blast, which is useful for the Voronoi step.

**Safety limit**: cap the search at MAX_ORE_CELLS (e.g., 10,000) to prevent
a giant ore deposit from freezing the game for multiple frames.

---

## 9. Ore Fragmentation — Voronoi Partition (v0.2)

### Goal
Split the collected ore cells into N irregular sub-fragments, like a rock
shattering into pieces.

### What is Voronoi?

Voronoi partitioning assigns every point in a space to its nearest "seed" point.
Given N seeds scattered across the ore region, each ore cell is assigned to the
nearest seed. Cells assigned to the same seed form one fragment.

```
Ore region (top view):          Voronoi seeds:     Resulting fragments:
■ ■ ■ ■ ■ ■                     ■ ■ ■ A ■ ■         1 1 1 2 2 2
■ ■ ■ ■ ■ ■    +  A, B, C  →    ■ B ■ ■ ■ ■    →    1 1 3 2 2 2
■ ■ ■ ■ ■ ■                     ■ ■ ■ ■ C ■         3 3 3 3 2 2
```

**How N is chosen**: `N = ceil(total_cells / FRAGMENT_THRESHOLD)`. A threshold of
~50 cells per fragment gives 3–6 pieces from a typical vein — feels right for gameplay.

**Seed placement**: random points within the ore region's bounding box that actually
fall on ore cells. Randomness makes every explosion look different.

### Fragment physical properties

After partitioning:
- Each fragment group becomes an `OreFragment` rigid body
- **Mass** = number of cells in the fragment × CELL_MASS_FACTOR
- **Initial velocity**: outward direction from blast center + random spread
- **Sprite**: the actual pixels from the terrain bitmap at those cells
  (the player sees the gold veins in the fragment — it's immediately legible)

---

## 10. Cellular Automaton Fluid (v0.2)

### Goal
Simulate water that fills terrain holes, flows through tunnels, and drains when
the floor is dug out. Similar to Terraria or Noita (simplified).

### How cells behave each tick

Every fluid cell holds a fill level from 0.0 (empty) to 1.0 (full).
Each tick, three rules apply in order:

**Rule 1 — Gravity (fall)**
If the cell below is less full, transfer fluid downward:
```
transfer = min(my_level, 1.0 - below_level)
my_level   -= transfer
below_level += transfer
```

**Rule 2 — Equalize (spread sideways)**
If left/right neighbors have less fluid and below is full:
```
average  = (my_level + neighbor_level) / 2
my_level = neighbor_level = average
```

**Rule 3 — Pressure (rise)**
If below is full and my level is full, push excess upward.
(This allows pipes and siphons — water rises to match its source height.)

**Solid boundary**: fluid checks the terrain bitmap. A fluid cell treats any
position with a solid terrain cell as a wall. This way, the two systems
(terrain 4px/cell, fluid 8px/cell) interact correctly without being the same grid.

**Why cellular automaton?** It's simple, fast, and produces satisfying emergent behavior
(flooding, draining, pressure, waterfalls) from just three local rules. The alternative —
full fluid dynamics (Navier-Stokes) — is accurate but prohibitively expensive for a
real-time game grid this size.

---

## 11. Field Diffusion (v0.2)

### Goal
Spread quantities like mana, heat, or radiation through the world. A mana crystal
emits mana into the local area; mages can harvest it; it slowly disperses over time.

### The diffusion equation (simplified)

Think of the field as a height map of water. High concentration spots slowly "pour"
into lower neighbors, and everything evaporates at a fixed rate.

```
new_F[x,y] = F[x,y] + dt * diffusion_rate * (
    F[x-1,y] + F[x+1,y] + F[x,y-1] + F[x,y+1] - 4 * F[x,y]
) - decay_rate * F[x,y]
```

Breaking it down:
- `(sum of 4 neighbors - 4 × center)`: positive if neighbors have more than center
  (field flows in), negative if center has more (field flows out)
- `diffusion_rate`: how fast it spreads (must obey `dt × rate × 4 < 1` or simulation explodes)
- `decay_rate`: how quickly the field naturally fades away

**Resolution**: 16px/cell grid — coarser than terrain (4px) for performance.
One diffusion cell covers 4×4 terrain cells.

**Stability condition**: `dt × diffusion_rate × 4 < 1`
At 60fps: `(1/60) × rate × 4 < 1` → `rate < 15`
We keep `rate` well under this limit per field type.

---

## 12. Portal Rendering (v0.3)

### Goal
A portal shows a "window" into another location. Looking through it, you see the
terrain and entities at the destination as if you were there.

### Virtual camera

A portal has a **transform** (position + rotation). The destination also has a transform.
To render what you'd see through the portal:

```
view_transform = destination_transform × inverse(source_transform) × main_camera
```

In plain terms: "take the camera's position relative to the source portal, then
apply that same relative position at the destination portal."

### Render steps

1. Compute the virtual camera transform (above formula)
2. Render the scene from the virtual camera into a **framebuffer** (offscreen texture)
3. Draw the portal's opening as a mask (stencil)
4. Blit the framebuffer through the mask — the portal opening shows the destination

**Recursion**: a portal can show another portal. We limit recursion to depth 1–2
(render at most 2 levels of portals inside portals) to avoid exponential cost.

---

## Appendix: Coordinate Systems

All positions in Aeterium use **world pixels** as the unit:

```
World pixels     → divided by CELL_SIZE (4) → Cell coordinates
Cell coordinates → divided by CHUNK_CELLS (64) → Chunk coordinates

World pixel (0, 0) = top-left corner of the map
Positive X = right
Positive Y = DOWN (screen-space convention)

Camera offset:
  screen_x = world_x - camera_offset.x
  screen_y = world_y - camera_offset.y
```

The positive-Y-downward convention matches SDL2's screen space, avoiding
sign flips in the rendering code.
