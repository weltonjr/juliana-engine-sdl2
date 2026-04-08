# Algorithms Reference

This document describes the core algorithms used in the Juliana engine. Each section
includes pseudocode and complexity analysis. Implementations live in the `src/` tree;
this file is the canonical reference for *how* and *why* each algorithm works.

Status markers: **[IMPL]** = implemented, **[PLANNED]** = designed but not yet coded.

---

## 1. Terrain Rendering [IMPL]

### Overview

The terrain is a flat `W x H` grid of `Cell` values (material ID + background ID). A
single SDL streaming texture covers the entire terrain. Rendering uses a source-rect /
dest-rect blit so only the camera-visible portion is sent to the GPU.

### Full Rebuild

Called once at map load. Locks the entire texture, iterates every cell, writes RGBA
pixels.

```
procedure FullRebuild(texture, terrain):
    lock texture -> pixels[], pitch
    for y in 0..H-1:
        for x in 0..W-1:
            cell = terrain.GetCell(x, y)
            color = ResolveCellColor(cell)
            pixels[y * pitch + x] = PackRGBA(color)
    unlock texture
```

`ResolveCellColor` checks the foreground material state: if the material is air (state =
None), it falls through to the background color; if the background is transparent (sky),
it returns the air color. Otherwise it returns the material color.

**Complexity:** O(W * H) -- runs once.

### Region Update

Called when terrain changes (digging, simulation). Only re-renders the affected
rectangle.

```
procedure UpdateRegion(texture, terrain, rx, ry, rw, rh):
    clamp (rx, ry, rw, rh) to terrain bounds
    lock texture sub-rect (rx, ry, rw, rh) -> pixels[], pitch
    for y in ry..ry+rh-1:
        for x in rx..rx+rw-1:
            cell = terrain.GetCell(x, y)
            color = ResolveCellColor(cell)
            pixels[(y-ry) * pitch + (x-rx)] = PackRGBA(color)
    unlock texture
```

**Complexity:** O(rw * rh) per dirty rect.

### Viewport Rendering

Each frame, the camera computes a source rect in terrain-pixel coordinates and the
renderer blits that portion to the screen.

```
procedure Render(renderer, texture, camera, terrain_w, terrain_h):
    src = camera.GetSourceRect(terrain_w, terrain_h)   // visible terrain region
    dst = {0, 0, viewport_w, viewport_h}               // full screen
    RenderCopy(renderer, texture, src, dst)             // GPU-scaled blit
```

The GPU handles upscaling (currently 2x). No per-pixel work is done at render time
beyond the single `SDL_RenderCopy`.

**Complexity:** O(1) CPU per frame (GPU does the scaling).

### Chunk-Based Rendering [IMPL]

The terrain is divided into 64x64 chunks, each with its own SDL texture. Only chunks
intersecting the camera viewport are drawn, and dirty chunks are rebuilt lazily (only
when they become visible). Color lookups use pre-built LUTs indexed by MaterialID/
BackgroundID to avoid registry hash lookups in the hot render loop.

```
CHUNK_SIZE = 64

procedure InitChunks(terrain):
    cols = ceil(W / CHUNK_SIZE)
    rows = ceil(H / CHUNK_SIZE)
    for cy in 0..rows-1:
        for cx in 0..cols-1:
            chunks[cy][cx].texture = CreateTexture(CHUNK_SIZE, CHUNK_SIZE)
            chunks[cy][cx].dirty = true

procedure RebuildDirtyChunks(terrain):
    for each chunk where chunk.dirty:
        lock chunk.texture -> pixels[], pitch
        for ly in 0..CHUNK_SIZE-1:
            for lx in 0..CHUNK_SIZE-1:
                wx = chunk.cx * CHUNK_SIZE + lx
                wy = chunk.cy * CHUNK_SIZE + ly
                cell = terrain.GetCell(wx, wy)
                pixels[ly * pitch + lx] = PackRGBA(ResolveCellColor(cell))
        unlock chunk.texture
        chunk.dirty = false

procedure RenderChunks(renderer, camera, chunks):
    view = camera.GetSourceRect()
    cx0 = floor(view.x / CHUNK_SIZE)
    cy0 = floor(view.y / CHUNK_SIZE)
    cx1 = ceil((view.x + view.w) / CHUNK_SIZE)
    cy1 = ceil((view.y + view.h) / CHUNK_SIZE)
    for cy in cy0..cy1:
        for cx in cx0..cx1:
            src = {0, 0, CHUNK_SIZE, CHUNK_SIZE}
            dst = WorldRectToScreen(cx * CHUNK_SIZE, cy * CHUNK_SIZE, CHUNK_SIZE, CHUNK_SIZE)
            RenderCopy(renderer, chunks[cy][cx].texture, src, dst)
```

**Complexity per frame:** O(V) where V = number of visible chunks (viewport area /
CHUNK_SIZE^2). For a 1280x720 window at 2x zoom: ~8x6 = 48 chunks max.

---

## 2. Powder Simulation [IMPL]

### Overview

Powder materials (sand, gravel) obey cellular-automaton gravity. The simulation runs
every `SIM_INTERVAL` (3) engine ticks to reduce CPU load.

### Algorithm

Scan bottom-to-top so that falling cells do not cascade within a single tick (each cell
moves at most one pixel per sim step).

```
procedure SimulatePowder(terrain):
    for y from H-2 down to 0:            // bottom-to-top, skip last row
        for x from 0 to W-1:
            cell = terrain.GetCell(x, y)
            if cell.state != Powder or not cell.gravity: continue

            below = terrain.GetCell(x, y+1)

            // Rule 1: Fall straight down into air or liquid
            if below.state == None or below.state == Liquid:
                swap(terrain[x,y], terrain[x,y+1])
                MarkDirty(x, y)
                MarkDirty(x, y+1)
                continue

            // Rule 2: Diagonal pile -- try both sides
            // Direction alternation: use (x+y) & 1 to pick initial side
            if (x + y) is odd:
                dirs = [+1, -1]
            else:
                dirs = [-1, +1]

            for d in dirs:
                nx = x + d
                if nx out of bounds: continue
                diag = terrain.GetCell(nx, y+1)
                side = terrain.GetCell(nx, y)
                if (diag.state == None or diag.state == Liquid) and
                   (side.state == None or side.state == Liquid):
                    swap(terrain[x,y], terrain[nx,y+1])
                    MarkDirty(x, y)
                    MarkDirty(nx, y+1)
                    break
```

### Key Design Decisions

- **Bottom-to-top scan** prevents a grain from falling multiple pixels in one tick.
- **Swap with liquid** means powder sinks through water naturally -- the displaced liquid
  cell moves up into the powder's old position.
- **Direction alternation** via `(x+y) & 1` produces symmetric piles. Without it, all
  grains would prefer one diagonal and piles would lean.
- **SIM_INTERVAL = 3** throttles the sim to ~20 Hz at 60 tick/s. This is a tuning knob.

### Chunk-Based Active Tracking [IMPL]

The simulation uses 64x64 sim-chunks with an `active` flag per chunk. Only chunks
flagged as active are scanned. Activity propagates:

- **Initial scan**: on first tick, all chunks are scanned and those containing
  powder/liquid with `gravity=true` are marked active.
- **MarkDirty propagation**: when a cell moves, its destination chunk and neighbors
  (below, left, right) are activated for the next tick.
- **External modification**: `NotifyModified(rect)` activates chunks around dig/blast
  sites so newly-exposed powder/liquid starts falling.
- **Deactivation**: if a chunk's scan produces no movement, it deactivates.

This reduces typical scan from O(W*H) to O(A * CHUNK_SIZE^2) where A is the number of
active chunks (usually <10 in steady state).

### Complexity

O(W * H) per sim step in the worst case (all cells are powder). In practice, chunk-based
active tracking reduces this to O(A * 4096) where A << total chunks. The inner check
short-circuits on non-powder cells.

---

## 3. Liquid Simulation [IMPL]

### Overview

Liquid materials (water, lava) fall under gravity and spread horizontally at a
configurable `flow_rate` (cells per sim step).

### Algorithm

Same bottom-to-top scan order as powder.

```
procedure SimulateLiquid(terrain):
    for y from H-2 down to 0:
        for x from 0 to W-1:
            cell = terrain.GetCell(x, y)
            if cell.state != Liquid or not cell.gravity: continue

            below = terrain.GetCell(x, y+1)

            // Rule 1: Fall straight down into air
            if below.state == None:
                swap(terrain[x,y], terrain[x,y+1])
                MarkDirty(x, y)
                MarkDirty(x, y+1)
                continue

            // Rule 2: Horizontal spread up to flow_rate cells
            flow = max(1, cell.flow_rate)
            if (x + y) is odd:
                dirs = [+1, -1]
            else:
                dirs = [-1, +1]

            for d in dirs:
                for step from 1 to flow:
                    nx = x + d * step
                    if nx out of bounds: break
                    side = terrain.GetCell(nx, y)
                    if side.state != None: break      // blocked
                    // Found empty cell -- move liquid there
                    swap(terrain[x,y], terrain[nx,y])
                    MarkDirty(x, y)
                    MarkDirty(nx, y)
                    goto next_cell
                // Try other direction if first was blocked
            next_cell:
```

### Key Design Decisions

- **Gravity first, spread second.** A liquid cell always tries to fall before spreading.
  This produces natural pooling behavior -- water fills from the bottom up.
- **Flow rate** controls viscosity: water might have `flow_rate = 3` (spreads fast),
  lava `flow_rate = 1` (sluggish).
- **Direction alternation** prevents liquid from always flowing left. The `(x+y) & 1`
  toggle ensures even spreading in both directions over successive ticks.
- **Only swaps with air.** Liquid does not displace other liquids or solids (unlike
  powder, which sinks through liquid).

### Complexity

O(W * H) per sim step worst case. The horizontal spread inner loop is bounded by
`flow_rate` (typically 1-5), so the constant factor is small.

---

## 4. Map Generation Pipeline [IMPL]

### Overview

Map generation is a 3-pass pipeline driven by scenario configuration. Each pass is
independent and operates on the `Terrain` grid in sequence.

```
procedure GenerateFromScenario(scenario, registry):
    rng = MersenneTwister(scenario.seed)
    terrain = Terrain(W, H)

    // Pass 1: Generate heightmap
    surface[] = GenerateShape(scenario.shape, W, H, rng, params)

    // Pass 2: Fill materials using rules
    AssignMaterials(terrain, surface, scenario.material_rules, registry)

    // Pass 3: Carve features
    ApplyFeatures(terrain, surface, scenario.features, registry, rng)

    return terrain
```

### Pass 1: Shape Generation

Each shape function returns a `surface[W]` array where `surface[x]` is the Y coordinate
of the terrain surface at column x. Everything above is sky; everything below is earth.

```
// Flat: sinusoidal noise around a base level
procedure GenerateFlatShape(W, H, rng, params):
    base = H * params.surface_level             // e.g. 0.35 -> top third is sky
    roughness = params.roughness
    for x in 0..W-1:
        wave = sin(x * 0.02) * 8
             + sin(x * 0.005) * 20              // large hills
             + sin(x * 0.05) * 3               // fine detail
        jitter = rng.uniform(-1, 1) * 2 * roughness
        surface[x] = clamp(base + wave * roughness + jitter, 2, H-2)
    return surface

// Island: parabolic profile, high center, drops to nothing at edges
procedure GenerateIslandShape(W, H, rng, params):
    sea_y = H * params.sea_level
    cx = W / 2
    for x in 0..W-1:
        dist = |x - cx| / cx                    // 0 at center, 1 at edges
        island = max(0, 1 - dist^2)             // parabolic dome
        h = island * params.terrain_height * H + small_waves + noise
        surface[x] = clamp(sea_y - h, 2, H-2)
    return surface

// Mountain: power-curve peak at center
procedure GenerateMountainShape(W, H, rng, params):
    cx = W / 2
    for x in 0..W-1:
        dist = |x - cx| / cx
        mountain = (1 - dist) ^ (1 + params.slope_steepness * 2)
        base = H * 0.8
        surface[x] = base - mountain * params.peak_height * H * 0.6 + waves + noise
    return surface

// Bowl: quadratic depression, high rim
procedure GenerateBowlShape(W, H, rng, params):
    cx = W / 2
    for x in 0..W-1:
        dist = |x - cx| / cx
        bowl = dist^2                            // 0 at center, 1 at rim
        h = params.rim_height * bowl + params.floor_depth * (1 - bowl)
        surface[x] = H * (1 - h) + waves + noise
    return surface
```

**Complexity:** O(W) per shape (linear scan).

### Pass 2: Material Assignment

Iterates every cell. For each cell, computes depth below the surface and tests an ordered
list of rules. First matching rule assigns the material and background.

```
procedure AssignMaterials(terrain, surface, rules, registry):
    for y in 0..H-1:
        for x in 0..W-1:
            surf = surface[x]
            depth = y - surf
            is_solid = (y >= surf)

            for rule in rules:                   // first-match wins
                match = false
                if rule.type == "above_surface" and not is_solid:
                    match = true
                else if rule.type == "surface_layer" and is_solid and depth < rule.depth:
                    match = true
                else if rule.type == "deep" and is_solid and depth >= rule.min_depth:
                    match = true
                else if rule.type == "fill" and is_solid:
                    match = true

                if match:
                    terrain.SetCell(x, y, {rule.material, rule.background})
                    break
```

**Complexity:** O(W * H * R) where R = number of rules (typically 3-6).

### Pass 3: Feature Carving

Features modify the terrain after material assignment. Each feature type has its own
generator.

```
// Caves: elliptical voids carved underground
procedure GenerateCaves(terrain, surface, config, rng):
    count = config.count or (W * H * config.density / 1000)
    for i in 0..count-1:
        cx = rng.uniform(20, W-20)
        min_y = surface[cx] + 15                 // minimum depth below surface
        cy = rng.uniform(min_y, H-10)
        rx = rng.uniform(config.min_size, config.max_size)
        ry = rx * 2 / 3                          // wider than tall
        for dy in -ry..ry:
            for dx in -rx..rx:
                if (dx/rx)^2 + (dy/ry)^2 > 1: continue   // ellipse test
                terrain.SetMaterial(cx+dx, cy+dy, Air)

// Ore Veins: circular blobs placed in matching zones
procedure GenerateOreVeins(terrain, surface, config, rng):
    count = config.count or (W * H * config.density / 100)
    for i in 0..count-1:
        cx = rng.uniform(0, W-1)
        cy = rng.uniform(0, H-1)
        surf = surface[cx]

        // Zone filter: "rock" = deep, "surface" = near top, "underground" = below surface
        if config.zone == "rock" and cy < surf + 40: continue
        if config.zone == "surface" and (cy < surf or cy > surf + 40): continue

        // Only replace existing solid cells
        if terrain.GetCell(cx, cy).state != Solid: continue

        radius = config.vein_radius/2 + rng.uniform(0, config.vein_radius)
        for dy in -radius..radius:
            for dx in -radius..radius:
                if dx^2 + dy^2 > radius^2: continue       // circle test
                if terrain.GetCell(cx+dx, cy+dy).state == Solid:
                    terrain.SetMaterial(cx+dx, cy+dy, config.ore_material)

// Lakes: rectangular water fills at the surface
procedure GenerateLakes(terrain, surface, config, rng):
    count = max(1, config.count)
    for i in 0..count-1:
        lx = rng.uniform(50, W-50)
        lake_w = rng.uniform(config.min_size, config.max_size)
        lake_d = lake_w / 3
        for x in lx..lx+lake_w-1:
            surf = surface[x]
            for y in surf..surf+lake_d-1:
                terrain.SetMaterial(x, y, Water)
```

**Complexity:** Caves O(count * rx * ry), Ore O(count * r^2), Lakes O(count * w * d).
Total generation is dominated by Pass 2: O(W * H).

---

## 5. Entity-Terrain Collision [IMPL]

### Overview

Entities use axis-aligned bounding boxes (AABB). Collision against terrain checks every
cell overlapping the AABB against a precomputed solid lookup table. Movement resolves X
and Y independently to allow clean wall sliding.

### Solid LUT

At startup, a 256-entry boolean table is built from material definitions:

```
procedure BuildSolidLUT(registry):
    for id in 0..255:
        mat = registry.GetMaterial(id)
        solid_lut[id] = (mat.state == Solid or mat.state == Powder)
```

This avoids hash-map lookups in the hot collision loop.

### Overlap Test

```
procedure CheckTerrainOverlap(terrain, x, y, w, h) -> bool:
    for py in y..y+h-1:
        for px in x..x+w-1:
            if solid_lut[terrain.GetCell(px, py).material_id]:
                return true
    return false
```

**Complexity:** O(w * h) per call. For a 12x20 character: 240 cell lookups.

### Movement Resolution

X and Y are resolved in separate passes. This is critical for smooth sliding behavior --
resolving both axes simultaneously would cause the entity to stick to walls.

```
procedure MoveEntity(entity, terrain, dt):
    save entity.prev_pos

    // --- X axis ---
    new_x = entity.pos_x + entity.vel_x * dt
    ix = new_x.ToInt()
    iy = entity.pos_y.ToInt()

    if CheckTerrainOverlap(terrain, ix, iy, entity.w, entity.h):
        // Try stepping up (walk over small bumps)
        step = TryStepUp(terrain, ix, iy, entity.w, entity.h, entity.step_up)
        if step > 0:
            entity.pos_x = new_x
            entity.pos_y -= step
        else:
            entity.vel_x = 0       // blocked -- stop horizontal movement
    else:
        entity.pos_x = new_x

    // --- Y axis ---
    new_y = entity.pos_y + entity.vel_y * dt
    ix = entity.pos_x.ToInt()       // use updated X
    new_iy = new_y.ToInt()

    if CheckTerrainOverlap(terrain, ix, new_iy, entity.w, entity.h):
        if entity.vel_y > 0:       // falling into ground
            entity.on_ground = true
            // Walk Y pixel by pixel to find exact landing position
            for test_y from current_iy to new_iy:
                if CheckTerrainOverlap(terrain, ix, test_y, entity.w, entity.h):
                    entity.pos_y = test_y - 1
                    break
        else:                       // rising into ceiling
            entity.pos_y = new_iy + 1
        entity.vel_y = 0
    else:
        entity.pos_y = new_y
        // Check ground contact (one row below feet)
        entity.on_ground = CheckTerrainOverlap(terrain, ix, new_iy + entity.h, entity.w, 1)
```

### Step-Up Traversal

When horizontal movement is blocked, the engine tests progressively higher positions
(up to `step_up` pixels, typically 4) to allow walking over small bumps and slopes
without jumping.

```
procedure TryStepUp(terrain, x, y, w, h, max_step) -> int:
    for step from 1 to max_step:
        if not CheckTerrainOverlap(terrain, x, y - step, w, h):
            return step             // found a clear position
    return 0                        // fully blocked
```

**Complexity:** O(max_step * w * h) worst case. With step_up=4 and a 12x20 entity:
4 * 240 = 960 cell checks. Runs once per moving entity per tick.

### Y-Axis Landing Search

When the entity is falling and collides, a linear scan finds the exact pixel where the
AABB first overlaps terrain. This prevents the entity from sinking into the ground.

```
for test_y from current_iy to new_iy:
    if CheckTerrainOverlap(terrain, ix, test_y, w, h):
        entity.pos_y = test_y - 1
        break
```

**Complexity:** O(delta_y * w * h) where delta_y = pixels fallen this tick. At 60 ticks/s
with max_fall_speed = 400, delta_y is at most ~7 pixels per tick.

---

## 6. Spawn Position Finding [IMPL]

### Overview

After map generation, the engine finds suitable spawn locations for each player slot.
The algorithm scores candidate positions based on flatness, sky clearance, water
avoidance, and distance from other spawns.

### Surface Scan

```
procedure FindSurfaceY(terrain, x, registry) -> int:
    for y from 0 to H-1:
        mat = registry.GetMaterial(terrain.GetCell(x, y).material_id)
        if mat.state == Solid or mat.state == Powder:
            return y
    return H - 1
```

### Spawn Search

```
procedure FindSpawnPositions(terrain, registry, player_slots) -> positions[]:
    search_step = max(1, W / 50)     // ~50 candidates across the map

    for each slot in player_slots:
        if slot.type == "none": push {0,0}; continue

        best = {W/2, FindSurfaceY(terrain, W/2) - 21}    // fallback: map center
        best_score = -1

        for x from 30 to W-30 step search_step:
            sy = FindSurfaceY(terrain, x)

            // 1. Flatness: count cells within min_flat_width where surface height
            //    differs by at most 2 pixels from center
            flat_count = 0
            for fx in (x - flat_width/2)..(x + flat_width/2):
                fy = FindSurfaceY(terrain, fx)
                if |fy - sy| <= 2: flat_count++

            // 2. Sky clearance: count unobstructed air cells above
            sky_above = 0
            for cy from sy-1 upward:
                if terrain.GetCell(x, cy).state == None: sky_above++
                else: break

            // 3. Water avoidance
            if slot.constraints.avoid_water:
                if terrain.GetCell(x, sy).state == Liquid: continue

            // 4. Distance from other players
            too_close = false
            for each prev in positions:
                if |prev.x - x| < slot.constraints.min_player_distance:
                    too_close = true; break
            if too_close: continue

            // Score: flatness weighted heavily, sky is secondary
            score = flat_count * 10 + sky_above
            if score > best_score:
                best_score = score
                best = {x, sy - 21}    // entity spawns above ground

        positions.push(best)

    return positions
```

**Complexity:** O(P * C * F) where P = player count, C = ~50 candidate positions,
F = flatness check width. Runs once at map load.

---

## 7. Camera System [IMPL]

### Coordinate Spaces

```
World space:   (0,0) = top-left of terrain, units = terrain pixels
Screen space:  (0,0) = top-left of window, units = screen pixels
Scale factor:  screen_pixels = world_pixels * scale (default 2.0)
```

### Viewport Geometry

```
view_world_width  = viewport_pixel_width / scale     // e.g. 1280/2 = 640
view_world_height = viewport_pixel_height / scale    // e.g.  720/2 = 360
```

The camera position `(x, y)` is the world-space coordinate of the viewport's top-left
corner.

### Coordinate Conversion

```
procedure WorldToScreen(wx, wy) -> (sx, sy):
    sx = (wx - camera.x) * scale
    sy = (wy - camera.y) * scale

procedure ScreenToWorld(sx, sy) -> (wx, wy):
    wx = camera.x + sx / scale
    wy = camera.y + sy / scale
```

### Smooth Follow

The camera tracks the player entity using exponential interpolation (lerp) each tick.
The lerp factor (0.1) controls responsiveness -- lower values produce smoother, more
cinematic camera movement; higher values snap more quickly.

```
procedure UpdateCameraFollow(camera, player, terrain):
    // Target: center player in viewport
    target_x = player.pos_x - camera.view_world_width / 2
    target_y = player.pos_y - camera.view_world_height / 2

    // Exponential ease (lerp factor = 0.1)
    FOLLOW_SPEED = 0.1
    camera.x += (target_x - camera.x) * FOLLOW_SPEED
    camera.y += (target_y - camera.y) * FOLLOW_SPEED

    // Clamp to terrain bounds (camera never shows outside the map)
    camera.x = clamp(camera.x, 0, terrain.width - camera.view_world_width)
    camera.y = clamp(camera.y, 0, terrain.height - camera.view_world_height)
```

The math behind the smooth follow: each tick the camera closes 10% of the gap to the
target. After n ticks, the remaining gap is `(1 - 0.1)^n = 0.9^n` of the original. This
converges exponentially -- the camera reaches 90% of target after ~22 ticks (0.37s at
60 tick/s). The feel is a soft chase that never overshoots.

### Source Rect for Rendering

```
procedure GetSourceRect(terrain_w, terrain_h) -> SDL_Rect:
    ix = floor(camera.x)
    iy = floor(camera.y)
    w = min(view_world_width, terrain_w - ix)
    h = min(view_world_height, terrain_h - iy)
    return {ix, iy, w, h}
```

**Complexity:** O(1) per frame.

---

## 8. Dirty-Rect Terrain Updates [IMPL]

### Overview

When terrain is modified (digging, powder/liquid simulation), only the changed region
needs re-rendering. The engine tracks a bounding dirty rectangle per simulation step.

### Tracking

The simulator maintains a running bounding box of all modified cells:

```
procedure MarkDirty(x, y):
    if no dirty cells yet:
        dirty_min = dirty_max = (x, y)
    else:
        dirty_min.x = min(dirty_min.x, x)
        dirty_min.y = min(dirty_min.y, y)
        dirty_max.x = max(dirty_max.x, x)
        dirty_max.y = max(dirty_max.y, y)
```

### Flush

At the end of each simulation step, the bounding box is expanded by 1 pixel on each
side (to catch visual edge artifacts) and emitted as a `DirtyRect`:

```
procedure FlushDirtyRects(terrain):
    if any_dirty:
        dirty_rects.push({
            x: max(0, dirty_min.x - 1),
            y: max(0, dirty_min.y - 1),
            w: min(W, dirty_max.x + 2) - max(0, dirty_min.x - 1),
            h: min(H, dirty_max.y + 2) - max(0, dirty_min.y - 1)
        })
    reset dirty state
```

### Engine Integration

The engine polls dirty rects after each sim step and calls `UpdateRegion` for each:

```
// In SimTick:
terrain_sim.Update(terrain)
if terrain_sim.HasChanges():
    for rect in terrain_sim.GetDirtyRects():
        terrain_renderer.UpdateRegion(rect.x, rect.y, rect.w, rect.h)
```

Digging also produces dirty rects directly:

```
// After DigCircle:
dug = terrain.DigCircle(cx, cy, radius, air_id)
if dug > 0:
    terrain_renderer.UpdateRegion(cx - radius - 1, cy - radius - 1,
                                   radius * 2 + 3, radius * 2 + 3)
```

### Current Limitation

The current implementation produces a single bounding rect that encompasses all changes
in a sim step. If powder falls in two distant columns, the rect spans both, causing
unnecessary pixel writes in the gap. A future optimization would track per-chunk dirty
flags instead (see Section 1, Future: Chunk-Based Rendering).

**Cost:** One `SDL_LockTexture` + pixel fill per dirty rect. The simulation typically
produces one bounding rect per tick; digging produces one per dig action.

---

## 9. Fixed-Point Arithmetic [IMPL]

### Format

Q16.16 signed fixed-point: 16 integer bits, 16 fractional bits, stored in `int32_t`.

```
ONE = 1 << 16 = 65536

Representable range: -32768.0 to +32767.99998 (approx)
Resolution: 1/65536 ~ 0.0000153
```

### Why Fixed-Point

Floating-point arithmetic is non-deterministic across platforms (x86 vs ARM, different
compiler flags, FMA fusion). For multiplayer lockstep synchronization, all clients must
produce identical simulation results. Fixed-point guarantees bitwise determinism because
integer arithmetic is specified exactly by the C++ standard.

### Conversions

```
procedure FromInt(value) -> Fixed:
    return value << 16

procedure FromFloat(value) -> Fixed:       // lossy -- used for config/display only
    return floor(value * 65536)

procedure ToInt(fixed) -> int:
    return fixed >> 16                      // truncates toward negative infinity

procedure ToFloat(fixed) -> float:         // lossy -- used for rendering interpolation
    return fixed / 65536.0
```

### Arithmetic

```
Add:      a.raw + b.raw                     // direct addition, same scale
Subtract: a.raw - b.raw                     // direct subtraction
Multiply: (int64(a.raw) * int64(b.raw)) >> 16
          // Widen to 64-bit to prevent overflow, then shift right
          // to restore Q16.16 from the Q32.32 intermediate
Divide:   (int64(a.raw) << 16) / int64(b.raw)
          // Shift numerator left first so fractional bits survive
Negate:   -a.raw
```

The multiply widens to `int64_t` because two Q16.16 values multiplied produce a Q32.32
result; shifting right by 16 extracts the Q16.16 portion. Division shifts the numerator
left first so the fractional bits survive the integer division.

### Overflow Considerations

- **Addition/subtraction:** can overflow if the result exceeds the 16-bit integer range
  (+/- 32767). Physics values are kept well within this range by design (positions are
  at most ~4096, velocities at most ~800).
- **Multiplication:** the int64 intermediate prevents overflow for any two Q16.16 values.
- **Division by zero:** undefined behavior, same as integer division. Callers must guard.

### Usage Rules

1. **All physics positions and velocities** use `Fixed`. Gravity, walk speed, jump
   velocity -- all stored as Fixed.
2. **Rendering** converts to `float` only at the interpolation step (between prev_pos
   and pos with the alpha factor). This is the only place floats appear in the
   simulation pipeline.
3. **Config values** (TOML properties) are read as `float` and immediately converted to
   Fixed via `FromFloat`.
4. **Never mix** fixed and float in physics calculations. The compiler will not catch
   this -- it is a discipline rule.

---

## 10. Explosion Fracture Algorithm [PLANNED]

### Overview

When an explosion detonates, it damages terrain in a radius, generates fracture lines
within material boundaries, identifies disconnected regions via flood fill, and spawns
each disconnected region as a dynamic terrain chunk entity.

### Phase 1: Blast Damage

Apply radial damage with linear falloff. Cells whose accumulated damage exceeds their
material hardness are destroyed (set to Air).

```
procedure ApplyBlastDamage(terrain, center, radius, power):
    destroyed = []
    for each cell (x, y) where distance(center, (x,y)) <= radius:
        dist = distance(center, (x, y))
        falloff = 1.0 - (dist / radius)          // linear falloff
        damage = power * falloff

        mat = terrain.GetCell(x, y).material
        if damage >= mat.hardness:
            terrain.SetMaterial(x, y, Air)         // fully destroyed
            destroyed.add((x, y))
        else:
            damage_map[x][y] += damage             // weaken for fracture propagation
```

**Complexity:** O(r^2) where r = blast radius.

### Phase 2: Fracture Line Generation

Fractures propagate outward from the blast center along random angles but stop at
material boundaries. This ensures chunks are single-material.

```
procedure GenerateFractures(terrain, center, radius, num_lines, rng):
    for i in 0..num_lines-1:
        angle = 2 * PI * i / num_lines + rng.uniform(-0.2, 0.2)
        // Walk outward from blast edge into surrounding terrain
        for t from radius to radius * 2:
            x = center.x + cos(angle) * t
            y = center.y + sin(angle) * t
            cell = terrain.GetCell(x, y)
            if cell.state == None: continue        // skip air gaps
            // Stop at material boundaries
            neighbor_mat = terrain.GetCell(x-1, y).material
            if cell.material != neighbor_mat and neighbor_mat != Air:
                break
            terrain.SetMaterial(x, y, Air)
```

**Complexity:** O(num_lines * r).

### Phase 3: Flood-Fill Disconnected Regions

After fractures are carved, scan the affected area for terrain regions that are no
longer connected to the main landmass.

```
procedure FindDisconnectedRegions(terrain, affected_rect) -> regions[]:
    visited = 2D bool array over affected_rect
    regions = []

    for each solid cell (x, y) in affected_rect:
        if visited[x][y]: continue
        region = FloodFill(terrain, x, y, visited, affected_rect)
        if not region.touches_border:
            regions.add(region)

    return regions

procedure FloodFill(terrain, start_x, start_y, visited, bounds) -> Region:
    queue = [(start_x, start_y)]
    region.material = terrain.GetCell(start_x, start_y).material
    region.touches_border = false
    region.cells = []

    while queue not empty:
        (x, y) = queue.dequeue()
        if visited[x][y]: continue
        cell = terrain.GetCell(x, y)
        if cell.state == None: continue
        if cell.material != region.material: continue    // single-material chunks

        visited[x][y] = true
        region.cells.add((x, y))

        // If this cell is at the edge of the affected area, the region
        // is still connected to the main landmass -- do not detach
        if (x, y) is on bounds boundary:
            region.touches_border = true

        for (nx, ny) in 4-neighbors of (x, y):
            if in bounds and not visited[nx][ny]:
                queue.enqueue((nx, ny))

    return region
```

**Complexity:** O(A) where A = area of the affected region (each cell visited once).

### Phase 4: Chunk Spawning

Each disconnected region is removed from the terrain grid and spawned as a dynamic
entity with pixel-mask collision.

```
procedure SpawnChunks(entity_manager, terrain, regions, blast_center, radius):
    MIN_FRAGMENT_PIXELS = from material definition

    for region in regions:
        mass = region.cell_count * region.material.density
        centroid = mean(region.cells)

        // Tiny fragments become item drops instead of physics chunks
        if region.cell_count < MIN_FRAGMENT_PIXELS:
            SpawnDigProduct(region.material, centroid)
            ClearRegionFromTerrain(region)
            continue

        // Build pixel mask in local coordinates
        bbox = bounding_box(region.cells)
        pixel_mask = bool[bbox.w][bbox.h], initialized to false
        for (x, y) in region.cells:
            pixel_mask[x - bbox.x][y - bbox.y] = true
            terrain.SetMaterial(x, y, Air)

        // Velocity: outward from blast center with distance falloff
        dir = normalize(centroid - blast_center)
        speed_factor = 1.0 - distance(centroid, blast_center) / (radius * 2)
        vel = dir * BLAST_SPEED * max(0, speed_factor)

        // Angular velocity from tangential component of blast force
        lever_arm = cross2D(centroid - blast_center, dir)
        angular_vel = lever_arm * ANGULAR_TRANSFER_FACTOR

        entity_manager.SpawnChunk({
            material:         region.material,
            pixel_mask:       pixel_mask,
            position:         centroid,
            velocity:         vel,
            angular_velocity: angular_vel,
            mass:             mass,
            physics_mode:     Dynamic,
            collision_shape:  PixelMask
        })
```

### Chunk Lifecycle

Once spawned, chunks are simulated by the standard physics system with additions:

1. **Rotation:** angular velocity integrated each tick, damped by `angular_drag` (0.98
   default). The pixel mask is rotated for collision checks.
2. **Pixel-mask collision:** instead of AABB overlap, the rotated pixel mask is tested
   against terrain cells. For each set pixel in the mask, transform to world coordinates
   and check the terrain grid.
3. **Settling:** when a chunk's linear speed drops below `SETTLE_THRESHOLD` and its
   angular velocity is near zero and it contacts terrain, the chunk "bakes" back into
   the grid. The pixel mask is stamped into the terrain at its current position and
   rotation, and the chunk entity is destroyed.
4. **Grabbing:** players can interact with a chunk to pick it up, entering the Carry
   action. This detaches the chunk from physics and attaches it to the player's hand
   point. Movement speed is reduced by `chunk.mass / player.carry_strength`.

```
procedure SettleChunk(chunk, terrain):
    if |chunk.vel| > SETTLE_THRESHOLD: return
    if |chunk.angular_vel| > ANGULAR_SETTLE_THRESHOLD: return
    if not chunk.touching_terrain: return

    // Bake pixels back into terrain
    for each set pixel (lx, ly) in chunk.pixel_mask:
        (wx, wy) = RotateAndTranslate(lx, ly, chunk.rotation, chunk.position)
        if terrain.InBounds(wx, wy) and terrain.GetCell(wx, wy).state == None:
            terrain.SetMaterial(wx, wy, chunk.material)

    MarkTerrainDirty(chunk.bounding_box_in_world)
    entity_manager.Destroy(chunk.entity_id)
```

**Complexity per tick:** Chunk physics is O(P) per chunk where P = pixel count in the
mask. Typical chunk sizes: 50-500 pixels. With a practical cap of ~20 active chunks,
total per-tick cost is at most ~10,000 pixel-terrain checks.

---

## Appendix A: Performance Lookup Tables

Several hot-path algorithms avoid per-cell hash-map lookups by building 256-entry lookup
tables at initialization, indexed directly by `MaterialID` (which is a `uint8_t`):

| Table | Type | Contents | Used By |
|---|---|---|---|
| `state_lut_` | `MaterialState[256]` | Material state enum | TerrainSimulator |
| `gravity_lut_` | `bool[256]` | Whether material has gravity | TerrainSimulator |
| `flow_lut_` | `int[256]` | Horizontal flow rate | TerrainSimulator |
| `solid_lut_` | `bool[256]` | Whether material blocks entities | PhysicsSystem |

These tables are rebuilt whenever the definition registry changes (currently only at
startup). The 256-entry limit comes from `MaterialID` being `uint8_t`.

---

## Appendix B: Entity Render Interpolation

The engine runs simulation at a fixed tick rate (60 Hz) but renders at the display
refresh rate. To prevent visual stuttering, entity positions are interpolated between
the previous and current simulation positions using the frame's alpha value.

```
procedure RenderEntity(entity, alpha):
    // alpha is in [0, 1], representing progress between last tick and next tick
    render_x = entity.prev_pos_x + (entity.pos_x - entity.prev_pos_x) * alpha
    render_y = entity.prev_pos_y + (entity.pos_y - entity.prev_pos_y) * alpha

    // Convert to screen coordinates
    (sx, sy) = camera.WorldToScreen(render_x, render_y)
    // Draw at (sx, sy) with scale applied
```

This conversion from Fixed to float happens here and only here. The simulation itself
never touches floating-point values.
