# Physics System

## Overview

The physics system handles gravity, movement, collision detection and response, buoyancy, and forces. All simulation uses **fixed-point Q16.16 arithmetic** for strict determinism. The physics system runs at a configurable fixed tick rate (default 60Hz), separate from the variable-rate renderer.

## Coordinate System

Standard 2D: **X** = right, **Y** = down (matches pixel coordinates and terrain grid). Origin (0,0) is top-left of the map. The terrain grid aligns 1:1 with the integer part of positions — an entity at position (100.5, 200.3) overlaps terrain cell [100, 200].

## Entity Physics Modes

| Mode | Gravity | Terrain Collision | Entity Collision | Moved By |
|---|---|---|---|---|
| `dynamic` | Yes | Yes — pushed out of terrain overlaps | If `solid=true`, pushes/is pushed by other solids | Forces, impulses, velocity, procedures |
| `static` | No | No (placed once, doesn't move) | If `solid=true`, other entities collide against it (immovable) | Nothing (fixed in place) |
| `kinematic` | No | No | If `solid=true`, pushes other entities but is not pushed by them | Script only (`SetPosition`, `SetVelocity`) |

**Dynamic**: characters, items, projectiles, debris — anything that falls and reacts to the world.
**Static**: buildings, anchored structures — placed once, never moved by physics.
**Kinematic**: moving platforms, elevators, doors — script-driven motion that affects other entities but isn't affected by them.

## Gravity

Gravity is a **world-level constant** configured in `server.toml`:

```toml
[simulation]
gravity = 800              # downward acceleration in fixed-point units per tick²
```

Applied every tick to all dynamic entities:

```
velocity.y += gravity * dt
if velocity.y > max_fall_speed then
    velocity.y = max_fall_speed
end
```

`max_fall_speed` is per-entity (from definition, overridable by actions).

## Entity-Terrain Collision

The core collision system. The terrain is a 1-pixel cell grid. Each entity has an AABB. Every tick:

1. Move entity by velocity (X and Y resolved separately for clean sliding)
2. Check if the AABB overlaps any solid/powder terrain cells
3. If X movement causes overlap:
   - Check step-up: if the obstacle is ≤ `step_up` pixels tall, shift entity up instead of stopping
   - Otherwise, push entity back to last valid X, zero horizontal velocity
4. If Y movement causes overlap:
   - Push entity back to last valid Y
   - If moving downward: entity is now on ground, zero vertical velocity
   - If moving upward: hit ceiling, zero vertical velocity

### Step-Up

Step-up allows entities to walk over small terrain bumps without stopping. Each entity defines its `step_up` height in the physics section (default 4, engine max configurable in `server.toml`).

When horizontal movement is blocked:
1. Check if raising the entity by 1..`step_up` pixels clears the obstacle
2. If yes, move entity up and continue horizontal movement
3. If no, entity is blocked — zero horizontal velocity

### Slope Handling

Entities walk at **constant speed** regardless of terrain incline. The `walk` procedure probes the terrain surface ahead and below to find the ground position, then sets the entity's Y to follow it. This prevents entities from bouncing down slopes or flying off hilltops.

Slopes steeper than a configurable threshold are treated as walls (entity cannot walk up, slides down instead).

## Entity-Entity Collision

Most entities **pass through each other**. Characters, items, and projectiles share the same space without physics interaction. Collision between entities has two independent systems:

### Physical Collision (solid flag)

Only entities with `solid = true` participate in physical collision response:

- **Dynamic vs Dynamic** (both solid): push apart proportional to mass. Heavier entity moves less.
- **Dynamic vs Static** (solid): dynamic entity is pushed out, static doesn't move.
- **Dynamic vs Kinematic** (solid): dynamic is pushed by kinematic, kinematic follows its script.

Most entities have `solid = false` (characters, items, projectiles). Solid entities are special: vehicles, large pushable objects.

### Overlap Detection

Entities with `overlap_detection = true` fire callbacks when their AABBs overlap, regardless of the `solid` flag:

```lua
-- Fires every tick while two entities overlap
OnOverlap(other_id)

-- Fires once when overlap starts
OnOverlapBegin(other_id)

-- Fires once when overlap ends
OnOverlapEnd(other_id)
```

This drives all gameplay interactions: picking up items (character overlaps item), entering buildings (character overlaps building), hitting enemies (projectile overlaps enemy), trigger zones, etc.

## Buildings

Buildings are **static, non-solid entities**. Characters walk through them. The building is a visual + script layer. Buildings have `overlap_detection = true` so scripts can detect characters entering/exiting.

### Building Placement

Buildings require flat terrain to be placed. The engine provides a terrain query for this:

```lua
-- Check if terrain is flat enough for a building at position (x, y) with given width
-- tolerance = max height difference in pixels across the width
local can_build = CheckFlatness(x, y, width, tolerance)

-- Find the surface Y at a column
local surface_y = GetSurfaceY(x)
```

The building's placement script checks flatness, and if valid, places the building entity at that position. No foundation cells are written to the terrain grid.

## Forces and Impulses

Two mechanisms for affecting entity movement:

### Impulse — Instant Velocity Change (Mass-Independent)

```lua
-- Adds directly to velocity, ignores mass
-- Used for: explosions, knockback, jumps
ApplyImpulse(entity_id, vx, vy)
-- Result: entity.velocity += (vx, vy)
```

Impulses are instant and one-shot. They're applied immediately to the entity's velocity. Same impulse gives same velocity change to a heavy rock and a light character.

### Force — Continuous Acceleration (Mass-Dependent)

```lua
-- Accumulated during the tick, applied at physics step
-- Used for: wind, magnets, conveyor belts, buoyancy
ApplyForce(entity_id, fx, fy)
-- Result at physics step: entity.velocity += (total_forces / mass) * dt
```

Forces are accumulated each tick, then converted to velocity change proportional to mass. A heavier entity accelerates less from the same force. Forces reset each tick — scripts re-apply them in `OnTick` for continuous effects.

```lua
-- Wind aspect — applies force every tick
function OnTick(dt)
    local entities = FindEntities(wind_x, wind_y, wind_radius, nil)
    for _, id in ipairs(entities) do
        ApplyForce(id, wind_strength, 0)
    end
end
```

## Buoyancy

The engine automatically calculates buoyancy for all dynamic entities overlapping liquid terrain cells:

```
submerged_cells = count of liquid cells inside entity AABB
total_cells = entity AABB width × height
submerged_ratio = submerged_cells / total_cells

liquid_density = liquid material's physics.density
entity_density = entity.mass / total_cells

buoyancy_force = (liquid_density - entity_density) * submerged_ratio * gravity
→ applied as upward force (negative Y)
```

| Entity vs Liquid | Result |
|---|---|
| Entity lighter than liquid | Floats (buoyancy > gravity) |
| Entity heavier than liquid | Sinks (buoyancy < gravity) |
| Roughly equal density | Hovers (neutral buoyancy) |

**Liquid drag** reduces velocity proportional to submersion:

```
velocity *= (1.0 - liquid_drag * submerged_ratio * dt)
```

Where `liquid_drag` is a property of the liquid material:

```toml
# In Water material definition
[behavior]
flow_rate = 3
liquid_drag = 0.85          # high drag
```

```toml
# In Lava material definition
[behavior]
flow_rate = 1
liquid_drag = 0.95          # very high drag
```

The `swim` procedure adds **player control** on top of engine buoyancy — input translates to forces/velocity changes that let the player move through liquid, but the underlying float/sink behavior comes from the engine.

## Fall Damage

The engine detects landing impacts and delegates behavior to Lua:

```
On landing (entity was in air, now touching ground):
  impact_speed = absolute vertical velocity at moment of contact
  → call OnFallImpact(impact_speed) on all entity aspects
```

The engine only detects and reports. Scripts decide what to do:

```lua
-- Combat/script.lua
function OnFallImpact(impact_speed)
    local threshold = GetProperty("fall_damage_threshold") or 300
    local scale = GetProperty("fall_damage_scale") or 0.5
    if impact_speed > threshold then
        local damage = (impact_speed - threshold) * scale
        self.hp = self.hp - damage
        PlaySound("fall_hurt")
        if self.hp <= 0 then
            SendMessage("died", { cause = "fall" })
        end
    end
end
```

Different entities handle falls differently — a cat takes no fall damage (doesn't define `OnFallImpact`), a character takes scaled damage, a glass bottle shatters on any impact.

## Collision Callbacks (Full List)

| Callback | When | Condition |
|---|---|---|
| `OnCollision(other_id, vx, vy)` | Two solid entities physically collide | At least one has `solid = true` |
| `OnOverlapBegin(other_id)` | AABB overlap starts | `overlap_detection = true` on both |
| `OnOverlap(other_id)` | AABB overlap continues (every tick) | `overlap_detection = true` on both |
| `OnOverlapEnd(other_id)` | AABB overlap ends | `overlap_detection = true` on both |
| `OnHitTerrain(material_id, x, y, vx, vy)` | Entity hits solid terrain during movement | Dynamic entity |
| `OnLiquidLevelChange(level, liquid_material)` | Ratio of liquid cells inside AABB changes | Dynamic entity, `level` = 0.0 to 1.0 |
| `OnFallImpact(impact_speed)` | Entity lands after being airborne | Dynamic entity |

## Physics Tick Order

Per simulation tick, the full sequence:

```
1.  Collect inputs (local keyboard/gamepad, network packets, AI)
2.  Run procedures for all entities (modify velocities based on input + movement mode)
3.  Run OnTick for all entities and scenario aspects (scripts may apply forces/impulses)
4.  Apply gravity to all dynamic entities
5.  Apply accumulated forces: velocity += (forces / mass) * dt
6.  Move entities by velocity:
    a. Move X — resolve terrain collision + step-up
    b. Move Y — resolve terrain collision
    c. Detect landing → fire OnFallImpact if applicable
7.  Calculate buoyancy for entities in liquid → apply as force for next tick
8.  Update liquid level for entities → fire OnLiquidLevelChange if changed
9.  Resolve entity-entity collisions (solid entities only — push apart)
10. Fire collision callbacks (OnCollision, OnOverlapBegin, OnOverlap, OnOverlapEnd, OnHitTerrain)
11. Simulate terrain (powder gravity, liquid flow, pressure equalization)
12. Apply pending terrain changes (batched)
13. Generate terrain diff
14. Process entity spawns and destroys (queued during tick)
15. Advance action animations (frame counting, transitions)
```

## Dynamic Object Rotation

Any dynamic entity can have rotation physics enabled. This supports rolling barrels, spinning projectiles, and tumbling terrain chunks.

### Definition

```toml
[physics]
rotation = true                # enable rotation (default: false)
moment_of_inertia = 0          # 0 = auto-calculate from mass + size
angular_drag = 0.98            # rotation damping multiplier per tick (1.0 = no drag)
bounce_angular_transfer = 0.3  # how much linear impact converts to spin on bounce (0–1)
```

### Runtime State

| Field | Type | Description |
|---|---|---|
| `angle` | fixed-point | Current rotation in degrees (0–360) |
| `angular_velocity` | fixed-point | Rotation speed (degrees per tick) |

### Rotation Sources

- **Terrain bounce**: hitting a surface at an angle converts some linear velocity into angular velocity (controlled by `bounce_angular_transfer`)
- **Off-center impulse**: `ApplyImpulseAt(entity_id, vx, vy, offset_x, offset_y)` — impulse applied away from center of mass generates torque
- **Torque**: `ApplyTorque(entity_id, torque)` — direct rotational force (mass-dependent, like `ApplyForce`)
- **Angular drag**: angular velocity is multiplied by `angular_drag` each tick, slowing rotation over time

### Rendering

Sprites (or pixel masks for chunks) are drawn rotated by `angle`. For collision, the AABB is kept axis-aligned — it expands to encompass the rotated shape. This keeps collision detection simple while giving visual fidelity.

### API

```lua
local angle = GetAngle(entity_id)
local angular_vel = GetAngularVelocity(entity_id)
SetAngularVelocity(entity_id, vel)
ApplyTorque(entity_id, torque)                     -- rotational force (mass-dependent)
ApplyImpulseAt(entity_id, vx, vy, offset_x, offset_y) -- off-center impulse → generates spin
```

---

## Dislodged Terrain Chunks

When explosions fracture terrain, disconnected regions break off as **dynamic entities** with pixel-based shapes. Chunks are created by the engine — they have no definition file; their data is constructed at runtime in C++.

### Chunk Data

| Field | Description |
|---|---|
| Pixel mask | Boolean grid — which pixels are solid (defines shape and collision) |
| Material ID | Single material for the entire chunk (fractures stay within one material) |
| Position | World position (fixed-point) |
| Velocity | Physics velocity (set by explosion force at creation) |
| Mass | `pixel_count × material.density` (calculated at creation) |
| AABB | Bounding box of the pixel mask |
| Rotation | Enabled — chunks tumble after explosions |

Chunks use `collision_shape = "pixel_mask"` — a special collision shape type where the solid pixels define the collision boundary. AABB is used for broadphase; pixel mask is used for narrowphase terrain collision.

### Explosion → Fracture → Chunks (Engine Built-in)

The entire explosion-to-chunk pipeline runs inside the C++ engine:

```
1. Explosion at (x, y) with radius and power
2. Apply blast damage to terrain cells in radius (cells closer to center take more damage)
3. For each material type in the affected area:
   a. Generate fracture lines within that material (cracks stop at material boundaries)
   b. Flood-fill from fracture lines to identify disconnected regions
4. For each disconnected region:
   a. Count pixels
   b. If pixels < material.min_fragment_pixels:
      → Spawn material.small_fragment as a normal object entity
        (e.g., GoldOre → spawns "GoldDust" item)
   c. If pixels >= material.min_fragment_pixels:
      → Create a DislodgedChunk entity:
        - Copy pixel mask from terrain
        - Calculate mass = pixel_count × material.density
        - Apply velocity outward from explosion center (proportional to power / mass)
        - Apply random angular velocity (spin from explosion)
5. Remove all affected cells from the terrain grid
```

### Material Definition Fields for Fragmentation

```toml
[material]
id = "GoldOre"

[behavior]
small_fragment = "base:GoldDust"   # object spawned for tiny fragments
min_fragment_pixels = 8            # below this → small_fragment; above → chunk
```

### Chunk Lifecycle

1. **Created** by explosion (engine built-in)
2. **Physics**: falls, bounces, tumbles with full dynamic physics + rotation
3. **Grab**: player overlaps chunk + presses interact → chunk attaches to player's `hand_r` point, player enters `Carry` action
4. **Carry**: chunk follows player position (attached, visible, not in container). Player speed reduced based on chunk mass.
5. **Deliver**: player drops chunk near a building's intake area → building script consumes it (destroys chunk entity, adds material value to building's internal state)
6. **Idle**: if nobody grabs it, chunk stays in the world as a persistent physics object

### Carrying Chunks

Characters can grab and carry one chunk at a time. Carrying is an action that reduces movement speed based on chunk mass:

```toml
# Character/animations.toml
[actions.Carry]
row = 9
frames = 6
delay = 4
next = "Carry"
procedure = "walk"
```

Speed reduction is calculated by the character's script based on the chunk's mass and the character's carry strength property.

### Building Intake

Buildings that process chunks define an intake area using a named point and a size:

```toml
# Smelter/definition.toml
[points]
intake = [16, 32]

[properties]
intake_radius = 12              # how close the chunk must be to be consumed
```

When a chunk is dropped (detached from a player) and overlaps a building's intake area, the building's script detects it via `OnOverlapBegin`, checks the material, and consumes it:

- `GetChunkMaterial(chunk_id)` — returns the material ID of the chunk
- `GetChunkPixelCount(chunk_id)` — returns the number of pixels (proportional to material amount)
- `Destroy(chunk_id)` — removes the chunk from the world

These three functions are the only chunk-specific Lua APIs. All other chunk behavior (physics, rotation, collision) uses the standard entity systems.

---

## Physics Engine API (Lua)

```lua
-- Position and velocity
local x, y = GetPosition(entity_id)
SetPosition(entity_id, x, y)
local vx, vy = GetVelocity(entity_id)
SetVelocity(entity_id, vx, vy)
SetVelocityX(entity_id, vx)
SetVelocityY(entity_id, vy)

-- Forces and impulses
ApplyImpulse(entity_id, vx, vy)     -- instant, mass-independent
ApplyForce(entity_id, fx, fy)       -- continuous, mass-dependent

-- Ground state
local grounded = IsOnGround(entity_id)
local material = GetGroundMaterial(entity_id)  -- material under feet, nil if airborne

-- Liquid state
local level = GetLiquidLevel(entity_id)        -- 0.0 to 1.0
local mat = GetLiquidMaterial(entity_id)       -- nil if dry

-- Terrain queries for placement
local can_build = CheckFlatness(x, y, width, tolerance)
local surface_y = GetSurfaceY(x)

-- Rotation (only for entities with rotation = true)
local angle = GetAngle(entity_id)
local angular_vel = GetAngularVelocity(entity_id)
SetAngularVelocity(entity_id, vel)
ApplyTorque(entity_id, torque)
ApplyImpulseAt(entity_id, vx, vy, offset_x, offset_y)

-- Chunk queries (only for dislodged chunks)
local material = GetChunkMaterial(chunk_id)
local pixels = GetChunkPixelCount(chunk_id)

-- Attachment (for carrying chunks or attaching visuals to points)
AttachToPoint(point_name, entity_id)
DetachFromPoint(point_name)
local attached = GetAttachedEntity(point_name)

-- Raycasting
local hit = Raycast(x1, y1, x2, y2)
-- Returns {x, y, material_id, entity_id} or nil
-- Hits terrain cells and solid entities

-- Physics properties
local mass = GetMass(entity_id)
SetMass(entity_id, mass)
```
